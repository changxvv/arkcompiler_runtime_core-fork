/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime/entrypoints/entrypoints.h"

#include "libpandabase/events/events.h"
#include "libpandabase/macros.h"
#include "compiler/compiler_options.h"
#include "runtime/deoptimization.h"
#include "runtime/arch/memory_helpers.h"
#include "runtime/jit/profiling_data.h"
#include "runtime/include/class_linker-inl.h"
#include "runtime/include/coretypes/array.h"
#include "runtime/include/exceptions.h"
#include "runtime/include/method-inl.h"
#include "runtime/include/object_header-inl.h"
#include "runtime/include/runtime.h"
#include "runtime/include/value-inl.h"
#include "runtime/include/panda_vm.h"
#include "runtime/interpreter/frame.h"
#include "runtime/interpreter/interpreter.h"
#include "runtime/interpreter/runtime_interface.h"
#include "runtime/mem/tlab.h"
#include "compiler/optimizer/ir/runtime_interface.h"
#include "runtime/handle_base-inl.h"
#include "libpandabase/utils/asan_interface.h"
#include "libpandabase/utils/tsan_interface.h"
#include "utils/cframe_layout.h"
#include "intrinsics.h"
#include "runtime/interpreter/vregister_iterator.h"

namespace panda {

using panda::compiler::TraceId;

#undef LOG_ENTRYPOINTS

class ScopedLog {
public:
    ScopedLog() = delete;
    explicit ScopedLog(const char *function) : function_(function)
    {
        LOG(DEBUG, INTEROP) << "ENTRYPOINT: " << function;
    }
    ~ScopedLog()
    {
        LOG(DEBUG, INTEROP) << "EXIT ENTRYPOINT: " << function_;
    }
    NO_COPY_SEMANTIC(ScopedLog);
    NO_MOVE_SEMANTIC(ScopedLog);

private:
    std::string function_;
};

#ifdef LOG_ENTRYPOINTS
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define LOG_ENTRYPOINT() ScopedLog __log(__FUNCTION__)
#else
#define LOG_ENTRYPOINT()
#endif

// enable stack walker dry run on each entrypoint to discover stack issues early
#ifndef NDEBUG
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CHECK_STACK_WALKER                                         \
    if (Runtime::GetOptions().IsVerifyEntrypoints()) {             \
        StackWalker::Create(ManagedThread::GetCurrent()).Verify(); \
    }
#else
#define CHECK_STACK_WALKER
#endif

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BEGIN_ENTRYPOINT() \
    CHECK_STACK_WALKER;    \
    LOG_ENTRYPOINT();

extern "C" NO_ADDRESS_SANITIZE void InterpreterEntryPoint(Method *method, Frame *frame)
{
    auto pc = method->GetInstructions();
    Method *callee = frame->GetMethod();
    ASSERT(callee != nullptr);

    if (callee->IsAbstract()) {
        ASSERT(pc == nullptr);
        panda::ThrowAbstractMethodError(callee);
        HandlePendingException();
        UNREACHABLE();
    }

    ManagedThread *thread = ManagedThread::GetCurrent();
    if (!thread->template StackOverflowCheck<true, false>()) {
        HandlePendingException(UnwindPolicy::SKIP_INLINED);
        UNREACHABLE();
    }

    Frame *prevFrame = thread->GetCurrentFrame();
    thread->SetCurrentFrame(frame);

    auto isCompiledCode = thread->IsCurrentFrameCompiled();
    thread->SetCurrentFrameIsCompiled(false);
    interpreter::Execute(thread, pc, frame);
    thread->SetCurrentFrameIsCompiled(isCompiledCode);
    if (prevFrame != nullptr && reinterpret_cast<uintptr_t>(prevFrame->GetMethod()) == COMPILED_CODE_TO_INTERPRETER) {
        thread->SetCurrentFrame(prevFrame->GetPrevFrame());
    } else {
        thread->SetCurrentFrame(prevFrame);
    }
}

extern "C" void AnnotateSanitizersEntrypoint([[maybe_unused]] void const *addr, [[maybe_unused]] size_t size)
{
#ifdef PANDA_TSAN_ON
    TSAN_ANNOTATE_HAPPENS_BEFORE(const_cast<void *>(addr));
#endif
#ifdef PANDA_ASAN_ON
    __asan_unpoison_memory_region(addr, size);
#endif
}

extern "C" void WriteTlabStatsEntrypoint([[maybe_unused]] void const *mem, size_t size)
{
    LOG_ENTRYPOINT();

    LOG(DEBUG, MM_OBJECT_EVENTS) << "Alloc object in compiled code at " << mem << " size: " << size;
    ASSERT(size <= Thread::GetCurrent()->GetVM()->GetHeapManager()->GetTLABMaxAllocSize());
    // 1. Pointer to TLAB
    // 2. Pointer to allocated memory
    // 3. size
    [[maybe_unused]] auto tlab = reinterpret_cast<size_t>(ManagedThread::GetCurrent()->GetTLAB());
    EVENT_TLAB_ALLOC(ManagedThread::GetCurrent()->GetId(), tlab, reinterpret_cast<size_t>(mem), size);
    if (mem::PANDA_TRACK_TLAB_ALLOCATIONS) {
        auto memStats = Thread::GetCurrent()->GetVM()->GetHeapManager()->GetMemStats();
        if (memStats == nullptr) {
            return;
        }
        memStats->RecordAllocateObject(size, SpaceType::SPACE_TYPE_OBJECT);
    }
}

extern "C" coretypes::Array *CreateArraySlowPathEntrypoint(Class *klass, size_t length)
{
    BEGIN_ENTRYPOINT();

    TSAN_ANNOTATE_HAPPENS_AFTER(klass);
    auto arr = coretypes::Array::Create(klass, length);
    if (UNLIKELY(arr == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    if (compiler::g_options.IsCompilerEnableTlabEvents()) {
        EVENT_SLOWPATH_ALLOC(ManagedThread::GetCurrent()->GetId());
    }
    return arr;
}

extern "C" coretypes::Array *CreateMultiArrayRecEntrypoint(ManagedThread *thread, Class *klass, uint32_t nargs,
                                                           size_t *sizes, uint32_t num)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic,-warnings-as-errors)
    auto arrSize = sizes[num];

    [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
    // SUPPRESS_CSA_NEXTLINE(alpha.core.CheckObjHeaderTypeRef)
    VMHandle<coretypes::Array> handle(thread, coretypes::Array::Create(klass, arrSize));
    if (handle.GetPtr() == nullptr) {
        return nullptr;
    }
    auto *component = klass->GetComponentType();

    if (component->IsArrayClass() && num + 1 < nargs) {
        for (size_t idx = 0; idx < arrSize; idx++) {
            auto *array = CreateMultiArrayRecEntrypoint(thread, component, nargs, sizes, num + 1);

            if (array == nullptr) {
                return nullptr;
            }
            handle.GetPtr()->template Set<coretypes::Array *>(idx, array);
        }
    }

    return handle.GetPtr();
}

extern "C" coretypes::String *CreateEmptyStringEntrypoint()
{
    BEGIN_ENTRYPOINT();

    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto str = coretypes::String::CreateEmptyString(vm->GetLanguageContext(), vm);
    if (UNLIKELY(str == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return str;
}

extern "C" coretypes::String *CreateStringFromStringEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto str = coretypes::String::CreateFromString(static_cast<coretypes::String *>(obj), vm->GetLanguageContext(), vm);
    if (UNLIKELY(str == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return str;
}

extern "C" coretypes::String *CreateStringFromCharsEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto array = static_cast<coretypes::Array *>(obj);
    auto str = coretypes::String::CreateNewStringFromChars(0, array->GetLength(), array, vm->GetLanguageContext(), vm);
    if (UNLIKELY(str == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return str;
}

extern "C" coretypes::String *CreateStringFromCharsWithOffsetEntrypoint(uint32_t offset, uint32_t length,
                                                                        ObjectHeader *obj,
                                                                        [[maybe_unused]] ObjectHeader *stringKlass)
{
    BEGIN_ENTRYPOINT();
    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto array = static_cast<coretypes::Array *>(obj);
    auto str = coretypes::String::CreateNewStringFromChars(offset, length, array, vm->GetLanguageContext(), vm);
    if (UNLIKELY(str == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return str;
}

extern "C" coretypes::String *CreateStringFromCharsZeroOffsetEntrypoint(uint32_t length, ObjectHeader *obj,
                                                                        [[maybe_unused]] ObjectHeader *stringKlass)
{
    BEGIN_ENTRYPOINT();
    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto array = static_cast<coretypes::Array *>(obj);
    auto str = coretypes::String::CreateNewStringFromChars(0, length, array, vm->GetLanguageContext(), vm);
    if (UNLIKELY(str == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return str;
}

extern "C" coretypes::String *SubStringFromStringEntrypoint(ObjectHeader *obj, int32_t begin, int32_t end)
{
    BEGIN_ENTRYPOINT();

    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto indexes = coretypes::String::NormalizeSubStringIndexes(begin, end, static_cast<coretypes::String *>(obj));
    auto substrLength = indexes.second - indexes.first;
    auto substr = coretypes::String::FastSubString(static_cast<coretypes::String *>(obj), indexes.first, substrLength,
                                                   vm->GetLanguageContext(), vm);
    if (UNLIKELY(substr == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return substr;
}

extern "C" coretypes::Array *StringGetCharsEntrypoint(ObjectHeader *obj, int32_t begin, int32_t end,
                                                      [[maybe_unused]] ObjectHeader *arrayKlass)
{
    BEGIN_ENTRYPOINT();

    auto length = static_cast<coretypes::String *>(obj)->GetLength();
    if (UNLIKELY(static_cast<uint32_t>(end) > length)) {
        ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
        panda::ThrowStringIndexOutOfBoundsException(end, length);
        HandlePendingException(UnwindPolicy::SKIP_INLINED);
        UNREACHABLE();
    }
    if (UNLIKELY(begin > end)) {
        ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
        panda::ThrowStringIndexOutOfBoundsException(begin, length);
        HandlePendingException(UnwindPolicy::SKIP_INLINED);
        UNREACHABLE();
    }

    if (UNLIKELY(begin < 0)) {
        ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
        panda::ThrowStringIndexOutOfBoundsException(begin, length);
        HandlePendingException(UnwindPolicy::SKIP_INLINED);
        UNREACHABLE();
    }
    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto arrayLength = end - begin;
    auto array = coretypes::String::GetChars(static_cast<coretypes::String *>(obj), begin, arrayLength,
                                             vm->GetLanguageContext());
    if (UNLIKELY(array == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return array;
}

extern "C" coretypes::Array *ResolveLiteralArrayEntrypoint(const Method *caller, uint32_t typeId)
{
    BEGIN_ENTRYPOINT();

    auto arr = Runtime::GetCurrent()->ResolveLiteralArray(ManagedThread::GetCurrent()->GetVM(), *caller, typeId);
    if (UNLIKELY(arr == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return arr;
}

extern "C" coretypes::Array *CreateMultiArrayEntrypoint(Class *klass, uint32_t nargs, size_t *sizes)
{
    BEGIN_ENTRYPOINT();

    auto arr = CreateMultiArrayRecEntrypoint(ManagedThread::GetCurrent(), klass, nargs, sizes, 0);
    if (UNLIKELY(arr == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return arr;
}

extern "C" ObjectHeader *CreateObjectByClassEntrypoint(Class *klass)
{
    BEGIN_ENTRYPOINT();

    // We need annotation here for the FullMemoryBarrier used in InitializeClassByIdEntrypoint
    TSAN_ANNOTATE_HAPPENS_AFTER(klass);
    if (klass->IsStringClass()) {
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*klass);
        auto str = coretypes::String::CreateEmptyString(ctx, Runtime::GetCurrent()->GetPandaVM());
        if (UNLIKELY(str == nullptr)) {
            HandlePendingException();
            UNREACHABLE();
        }
        return str;
    }

    if (LIKELY(klass->IsInstantiable())) {
        auto obj = ObjectHeader::Create(klass);
        if (UNLIKELY(obj == nullptr)) {
            HandlePendingException();
            UNREACHABLE();
        }
        if (compiler::g_options.IsCompilerEnableTlabEvents()) {
            EVENT_SLOWPATH_ALLOC(ManagedThread::GetCurrent()->GetId());
        }
        return obj;
    }

    ThrowInstantiationErrorEntrypoint(klass);
    UNREACHABLE();
}

extern "C" ObjectHeader *CloneObjectEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();

    uint32_t flags = obj->ClassAddr<Class>()->GetFlags();
    if (UNLIKELY((flags & Class::IS_CLONEABLE) == 0)) {
        ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
        ThrowCloneNotSupportedException();
        HandlePendingException(UnwindPolicy::SKIP_INLINED);
        return nullptr;
    }
    return ObjectHeader::Clone(obj);
}

extern "C" ObjectHeader *PostBarrierWriteEntrypoint(ObjectHeader *obj, size_t size)
{
    LOG_ENTRYPOINT();
    AnnotateSanitizersEntrypoint(obj, size);
    auto *objectClass = obj->ClassAddr<Class>();
    auto *barrierSet = ManagedThread::GetCurrent()->GetBarrierSet();
    if (!objectClass->IsArrayClass() || !objectClass->GetComponentType()->IsPrimitive()) {
        barrierSet->PostBarrier(obj, 0, size);
    }
    return obj;
}

extern "C" void CheckCastEntrypoint(const ObjectHeader *obj, Class *klass)
{
    BEGIN_ENTRYPOINT();

    // Don't use obj after ClassLinker call because GC can move it.
    // Since we need only class and class in a non-movalble object
    // it is ok to get it here.
    Class *objKlass = obj == nullptr ? nullptr : obj->ClassAddr<Class>();
    if (UNLIKELY(objKlass != nullptr && !klass->IsAssignableFrom(objKlass))) {
        panda::ThrowClassCastException(klass, objKlass);
        HandlePendingException();
        UNREACHABLE();
    }
}

extern "C" uint8_t IsInstanceEntrypoint(ObjectHeader *obj, Class *klass)
{
    BEGIN_ENTRYPOINT();

    // Don't use obj after ClassLinker call because GC can move it.
    // Since we need only class and class in a non-movalble object
    // it is ok to get it here.
    Class *objKlass = obj == nullptr ? nullptr : obj->ClassAddr<Class>();
    if (UNLIKELY(objKlass != nullptr && klass->IsAssignableFrom(objKlass))) {
        return 1;
    }
    return 0;
}

extern "C" void SafepointEntrypoint()
{
    BEGIN_ENTRYPOINT();
    interpreter::RuntimeInterface::Safepoint();
}

extern "C" ObjectHeader *ResolveClassObjectEntrypoint(const Method *caller, FileEntityId typeId)
{
    BEGIN_ENTRYPOINT();
    auto klass = reinterpret_cast<Class *>(ResolveClassEntrypoint(caller, typeId));
    return klass->GetManagedObject();
}

extern "C" void *ResolveClassEntrypoint(const Method *caller, FileEntityId typeId)
{
    BEGIN_ENTRYPOINT();
    ClassLinker *classLinker = Runtime::GetCurrent()->GetClassLinker();
    Class *klass = classLinker->GetClass(*caller, panda_file::File::EntityId(typeId));
    if (UNLIKELY(klass == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return reinterpret_cast<void *>(klass);
}

extern "C" coretypes::String *ResolveStringEntrypoint(const Method *caller, FileEntityId id)
{
    BEGIN_ENTRYPOINT();
    return Runtime::GetCurrent()->ResolveStringFromCompiledCode(ManagedThread::GetCurrent()->GetVM(), *caller,
                                                                panda_file::File::EntityId(id));
}

extern "C" coretypes::String *ResolveStringAotEntrypoint(const Method *caller, FileEntityId id, ObjectHeader **slot)
{
    BEGIN_ENTRYPOINT();
    auto runtime = Runtime::GetCurrent();
    auto aotManager = runtime->GetClassLinker()->GetAotManager();
    auto vm = ManagedThread::GetCurrent()->GetVM();
    auto str = runtime->ResolveStringFromCompiledCode(vm, *caller, panda::panda_file::File::EntityId(id));
    if (UNLIKELY(str == nullptr)) {
        return nullptr;
    }

    // to many strings were saved to slots, so simply return the resolved string
    if (aotManager->GetAotStringRootsCount() >= Runtime::GetOptions().GetAotStringGcRootsLimit()) {
        return str;
    }

    auto counter = reinterpret_cast<std::atomic_uint32_t *>(slot);

    // Atomic with acquire order reason: data race with slot with dependecies on reads after the load which should
    // become visible
    auto counterVal = counter->load(std::memory_order_acquire);
    if (counterVal >= compiler::RuntimeInterface::RESOLVE_STRING_AOT_COUNTER_LIMIT - 1) {
        return str;
    }

    if (counterVal < Runtime::GetOptions().GetResolveStringAotThreshold()) {
        // try to update counter, but ignore result - in the worst case we'll save
        // string's pointer to slot a bit later
        counter->compare_exchange_strong(counterVal, counterVal + 1, std::memory_order_release,
                                         std::memory_order_relaxed);
    } else {
        // try to replace the counter with string pointer and register the slot as GC root in case of success
        if (counter->compare_exchange_strong(counterVal, static_cast<uint32_t>(reinterpret_cast<uint64_t>(str)),
                                             std::memory_order_release, std::memory_order_relaxed)) {
            bool isYoung = vm->GetHeapManager()->IsObjectInYoungSpace(str);
            aotManager->RegisterAotStringRoot(slot, isYoung);
            EVENT_AOT_RESOLVE_STRING(ConvertToString(str));
        }
    }

    return str;
}

extern "C" Frame *CreateFrameWithSize(uint32_t size, uint32_t nregs, Method *method, Frame *prev)
{
    uint32_t extSz = EMPTY_EXT_FRAME_DATA_SIZE;
    if (LIKELY(method)) {
        extSz = Runtime::GetCurrent()->GetLanguageContext(*method).GetFrameExtSize();
    }
    size_t allocSz = Frame::GetAllocSize(size, extSz);
    Frame *frame = Thread::GetCurrent()->GetVM()->GetHeapManager()->AllocateExtFrame(allocSz, extSz);
    if (UNLIKELY(frame == nullptr)) {
        return nullptr;
    }
    return new (frame) Frame(Frame::ToExt(frame, extSz), method, prev, nregs);
}

extern "C" Frame *CreateFrameWithActualArgsAndSize(uint32_t size, uint32_t nregs, uint32_t numActualArgs,
                                                   Method *method, Frame *prev)
{
    auto *thread = ManagedThread::GetCurrent();
    uint32_t extSz = thread->GetVM()->GetFrameExtSize();
    size_t allocSz = Frame::GetAllocSize(size, extSz);
    void *mem = thread->GetStackFrameAllocator()->Alloc(allocSz);
    if (UNLIKELY(mem == nullptr)) {
        return nullptr;
    }
    return new (Frame::FromExt(mem, extSz)) Frame(mem, method, prev, nregs, numActualArgs);
}

extern "C" Frame *CreateNativeFrameWithActualArgsAndSize(uint32_t size, uint32_t nregs, uint32_t numActualArgs,
                                                         Method *method, Frame *prev)
{
    uint32_t extSz = EMPTY_EXT_FRAME_DATA_SIZE;
    size_t allocSz = Frame::GetAllocSize(size, extSz);
    void *mem = ManagedThread::GetCurrent()->GetStackFrameAllocator()->Alloc(allocSz);
    if (UNLIKELY(mem == nullptr)) {
        return nullptr;
    }
    return new (Frame::FromExt(mem, extSz)) Frame(mem, method, prev, nregs, numActualArgs);
}

template <bool IS_DYNAMIC = false>
static Frame *CreateFrame(uint32_t nregs, Method *method, Frame *prev)
{
    return CreateFrameWithSize(Frame::GetActualSize<IS_DYNAMIC>(nregs), nregs, method, prev);
}

template <bool IS_DYNAMIC>
static Frame *CreateFrameWithActualArgs(uint32_t nregs, uint32_t numActualArgs, Method *method, Frame *prev)
{
    auto frame =
        CreateFrameWithActualArgsAndSize(Frame::GetActualSize<IS_DYNAMIC>(nregs), nregs, numActualArgs, method, prev);
    if (IS_DYNAMIC) {
        LanguageContext ctx = Runtime::GetCurrent()->GetLanguageContext(*method);
        coretypes::TaggedValue initialValue = ctx.GetInitialTaggedValue();
        for (size_t i = 0; i < nregs; i++) {
            frame->GetVReg(i).SetValue(initialValue.GetRawData());
        }
        frame->SetDynamic();
    }
    return frame;
}

extern "C" Frame *CreateFrameForMethod(Method *method, Frame *prev)
{
    auto nregs = method->GetNumArgs() + method->GetNumVregs();
    return CreateFrame<false>(nregs, method, prev);
}

extern "C" Frame *CreateFrameForMethodDyn(Method *method, Frame *prev)
{
    auto nregs = method->GetNumArgs() + method->GetNumVregs();
    return CreateFrame<true>(nregs, method, prev);
}

extern "C" Frame *CreateFrameForMethodWithActualArgs(uint32_t numActualArgs, Method *method, Frame *prev)
{
    auto nargs = std::max(numActualArgs, method->GetNumArgs());
    auto nregs = nargs + method->GetNumVregs();
    return CreateFrameWithActualArgs<false>(nregs, numActualArgs, method, prev);
}

extern "C" Frame *CreateFrameForMethodWithActualArgsDyn(uint32_t numActualArgs, Method *method, Frame *prev)
{
    auto nargs = std::max(numActualArgs, method->GetNumArgs());
    auto nregs = nargs + method->GetNumVregs();
    return CreateFrameWithActualArgs<true>(nregs, numActualArgs, method, prev);
}

extern "C" void FreeFrame(Frame *frame)
{
    ASSERT(frame->GetExt() != nullptr);
    ManagedThread::GetCurrent()->GetStackFrameAllocator()->Free(frame->GetExt());
}

extern "C" uintptr_t GetStaticFieldAddressEntrypoint(Method *method, uint32_t fieldId)
{
    BEGIN_ENTRYPOINT();
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto field = classLinker->GetField(*method, panda_file::File::EntityId(fieldId));
    if (UNLIKELY(field == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    auto *klass = field->GetClass();
    ASSERT(klass != nullptr);
    return reinterpret_cast<uintptr_t>(klass) + field->GetOffset();
}

extern "C" void UnresolvedStoreStaticBarrieredEntrypoint(Method *method, uint32_t fieldId, ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    auto thread = ManagedThread::GetCurrent();
    // One must not use plain ObjectHeader pointer here as GC can move obj.
    // Wrap it in VMHandle.
    [[maybe_unused]] HandleScope<ObjectHeader *> objHandleScope(thread);
    VMHandle<ObjectHeader> objHandle(thread, obj);

    auto field = Runtime::GetCurrent()->GetClassLinker()->GetField(*method, panda_file::File::EntityId(fieldId));
    if (UNLIKELY(field == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    if (UNLIKELY(!field->GetType().IsReference())) {
        HandlePendingException();
        UNREACHABLE();
    }

    auto *classPtr = field->GetClass();
    ASSERT(classPtr != nullptr);
    InitializeClassEntrypoint(classPtr);

    // field_addr = class_ptr + field's offset
    auto fieldAddr = reinterpret_cast<uint32_t *>(reinterpret_cast<uintptr_t>(classPtr) + field->GetOffset());
    auto *barrierSet = thread->GetBarrierSet();
    // Pre-barrier
    if (barrierSet->IsPreBarrierEnabled()) {
        // Load
        // Atomic with acquire order reason: assume load can be volatile
        auto currentReference = __atomic_load_n(fieldAddr, std::memory_order_acquire);
        barrierSet->PreBarrier(reinterpret_cast<void *>(currentReference));
    }
    // Store
    // Atomic with release order reason: assume store can be volatile
    auto ref = reinterpret_cast<uintptr_t>(objHandle.GetPtr());
    ASSERT(sizeof(ref) == 4U || (ref & 0xFFFFFFFF00000000UL) == 0);
    __atomic_store_n(fieldAddr, static_cast<uint32_t>(ref), std::memory_order_release);
    // Post-barrier
    if (!mem::IsEmptyBarrier(barrierSet->GetPostType())) {
        auto objPtrPtr = reinterpret_cast<uintptr_t>(classPtr) + Class::GetManagedObjectOffset();
        auto objPtr = *reinterpret_cast<uint32_t *>(objPtrPtr);
        barrierSet->PostBarrier(reinterpret_cast<void *>(objPtr), 0, reinterpret_cast<void *>(ref));
    }
}

extern "C" uintptr_t GetUnknownStaticFieldMemoryAddressEntrypoint(Method *method, uint32_t fieldId, size_t *slot)
{
    BEGIN_ENTRYPOINT();
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto field = classLinker->GetField(*method, panda_file::File::EntityId(fieldId));
    if (UNLIKELY(field == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    auto *klass = field->GetClass();
    ASSERT(klass != nullptr);
    InitializeClassEntrypoint(klass);

    uintptr_t addr = reinterpret_cast<uintptr_t>(klass) + field->GetOffset();
    if (klass->IsInitialized() && slot != nullptr) {
        *slot = addr;
    }
    return addr;
}

extern "C" size_t GetFieldOffsetEntrypoint(Method *method, uint32_t fieldId)
{
    BEGIN_ENTRYPOINT();
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto field = classLinker->GetField(*method, panda_file::File::EntityId(fieldId));
    if (UNLIKELY(field == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    return field->GetOffset();
}

extern "C" void InitializeClassEntrypoint(Class *klass)
{
    BEGIN_ENTRYPOINT();
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    if (!klass->IsInitialized() && !classLinker->InitializeClass(ManagedThread::GetCurrent(), klass)) {
        HandlePendingException();
        UNREACHABLE();
    }
}

extern "C" Class *InitializeClassByIdEntrypoint(const Method *caller, FileEntityId id)
{
    BEGIN_ENTRYPOINT();
    ClassLinker *classLinker = Runtime::GetCurrent()->GetClassLinker();
    Class *klass = classLinker->GetClass(*caller, panda_file::File::EntityId(id));
    if (UNLIKELY(klass == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    // Later we store klass pointer into .aot_got section.
    // Without full memory barrier on the architectures with weak memory order
    // we can read klass pointer, but fetch klass data before it's set in GetClass and InitializeClass
    arch::FullMemoryBarrier();
    // Full barrier is not visible by TSAN so we need annotation here
    TSAN_ANNOTATE_HAPPENS_BEFORE(klass);
    InitializeClassEntrypoint(klass);
    return klass;
}

extern "C" uintptr_t NO_ADDRESS_SANITIZE ResolveVirtualCallEntrypoint(const Method *callee, ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    if (UNLIKELY(callee == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    auto *resolved = obj->ClassAddr<Class>()->ResolveVirtualMethod(callee);
    ASSERT(resolved != nullptr);

    return reinterpret_cast<uintptr_t>(resolved);
}

extern "C" uintptr_t NO_ADDRESS_SANITIZE ResolveVirtualCallAotEntrypoint(const Method *caller, ObjectHeader *obj,
                                                                         size_t calleeId,
                                                                         [[maybe_unused]] uintptr_t cacheAddr)
{
    BEGIN_ENTRYPOINT();
    // Don't use obj after ClassLinker call because GC can move it.
    // Since we need only class and class in a non-movalble object
    // it is ok to get it here.
    auto *objKlass = obj->ClassAddr<Class>();
    Method *method = Runtime::GetCurrent()->GetClassLinker()->GetMethod(*caller, panda_file::File::EntityId(calleeId));
    if (UNLIKELY(method == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    auto *resolved = objKlass->ResolveVirtualMethod(method);
    ASSERT(resolved != nullptr);

#if defined(PANDA_TARGET_ARM64)
    // In arm64, use interface inlineCache
    // NOTE(liyiming): will support x86_64 in future
    // issue #7018
    auto methodHead = objKlass->GetRawFirstMethodAddr();
    if (cacheAddr == 0 || methodHead == nullptr) {
        return reinterpret_cast<uintptr_t>(resolved);
    }

    constexpr uint32_t METHOD_COMPRESS = 3;
    constexpr uint32_t CACHE_OFFSET_32 = 32;
    constexpr uint32_t CACHE_OFFSET_34 = 34;
    auto cache = reinterpret_cast<int64_t *>(cacheAddr);
    auto methodResolved = reinterpret_cast<int64_t>(resolved);
    int64_t methodCache = methodResolved - reinterpret_cast<int64_t>(methodHead);

    int64_t methodCacheJudge = methodCache >> CACHE_OFFSET_34;  // NOLINT(hicpp-signed-bitwise)
    if (methodCacheJudge != 0 && methodCacheJudge != -1) {
        return reinterpret_cast<uintptr_t>(resolved);
    }
    methodCache = methodCache >> METHOD_COMPRESS;                           // NOLINT(hicpp-signed-bitwise)
    methodCache = methodCache << CACHE_OFFSET_32;                           // NOLINT(hicpp-signed-bitwise)
    int64_t saveCache = methodCache | reinterpret_cast<int64_t>(objKlass);  // NOLINT(hicpp-signed-bitwise)
    *cache = saveCache;
#endif
    return reinterpret_cast<uintptr_t>(resolved);
}

extern "C" uintptr_t NO_ADDRESS_SANITIZE ResolveUnknownVirtualCallEntrypoint(const Method *caller, ObjectHeader *obj,
                                                                             size_t calleeId, size_t *slot)
{
    {
        auto thread = ManagedThread::GetCurrent();
        [[maybe_unused]] HandleScope<ObjectHeader *> scope(thread);
        VMHandle<ObjectHeader> handleObj(thread, obj);

        BEGIN_ENTRYPOINT();
        auto runtime = Runtime::GetCurrent();
        Method *method = runtime->GetClassLinker()->GetMethod(*caller, panda_file::File::EntityId(calleeId));
        if (LIKELY(method != nullptr)) {
            // Cache a method index in vtable
            if (slot != nullptr && (!method->GetClass()->IsInterface() || method->IsDefaultInterfaceMethod())) {
                // We save 'vtable index + 1' because 0 value means uninitialized.
                // Codegen must subtract index after loading from the slot.
                *slot = method->GetVTableIndex() + 1;
            }

            auto *resolved = handleObj.GetPtr()->ClassAddr<Class>()->ResolveVirtualMethod(method);
            ASSERT(resolved != nullptr);

            return reinterpret_cast<uintptr_t>(resolved);
        }
    }

    HandlePendingException();
    UNREACHABLE();
}

extern "C" void CheckStoreArrayReferenceEntrypoint(coretypes::Array *array, ObjectHeader *storeObj)
{
    BEGIN_ENTRYPOINT();
    ASSERT(array != nullptr);
    ASSERT(storeObj != nullptr);

    // SUPPRESS_CSA_NEXTLINE(alpha.core.WasteObjHeader)
    auto *arrayClass = array->ClassAddr<Class>();
    auto *elementClass = arrayClass->GetComponentType();
    // SUPPRESS_CSA_NEXTLINE(alpha.core.WasteObjHeader)
    if (UNLIKELY(!storeObj->IsInstanceOf(elementClass))) {
        // SUPPRESS_CSA_NEXTLINE(alpha.core.WasteObjHeader)
        panda::ThrowArrayStoreException(arrayClass, storeObj->ClassAddr<Class>());
        HandlePendingException();
        UNREACHABLE();
    }
}

extern "C" Method *GetCalleeMethodEntrypoint(const Method *caller, size_t calleeId)
{
    BEGIN_ENTRYPOINT();
    auto *method = Runtime::GetCurrent()->GetClassLinker()->GetMethod(*caller, panda_file::File::EntityId(calleeId));
    if (UNLIKELY(method == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }

    return method;
}

extern "C" Method *GetUnknownCalleeMethodEntrypoint(const Method *caller, size_t calleeId, size_t *slot)
{
    BEGIN_ENTRYPOINT();
    auto *method = Runtime::GetCurrent()->GetClassLinker()->GetMethod(*caller, panda_file::File::EntityId(calleeId));
    if (UNLIKELY(method == nullptr)) {
        HandlePendingException();
        UNREACHABLE();
    }
    auto klass = method->GetClass();
    ASSERT(klass != nullptr);
    InitializeClassEntrypoint(klass);

    if (klass->IsInitialized() && slot != nullptr) {
        *slot = reinterpret_cast<size_t>(method);
    }

    return method;
}

extern "C" void SetExceptionEvent([[maybe_unused]] events::ExceptionType type)
{
#ifdef PANDA_EVENTS_ENABLED
    auto stack = StackWalker::Create(ManagedThread::GetCurrent());
    EVENT_EXCEPTION(std::string(stack.GetMethod()->GetFullName()), stack.GetBytecodePc(), stack.GetNativePc(), type);
#endif
}

extern "C" NO_ADDRESS_SANITIZE void ThrowExceptionEntrypoint(ObjectHeader *exception)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "ThrowExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    if (exception == nullptr) {
        NullPointerExceptionEntrypoint();
        UNREACHABLE();
    }
    ManagedThread::GetCurrent()->SetException(exception);

    SetExceptionEvent(events::ExceptionType::THROW);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void ThrowNativeExceptionEntrypoint()
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "ThrowNativeExceptionEntrypoint \n";
    SetExceptionEvent(events::ExceptionType::NATIVE);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void ArrayIndexOutOfBoundsExceptionEntrypoint([[maybe_unused]] ssize_t idx,
                                                                             [[maybe_unused]] size_t length)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "ArrayIndexOutOfBoundsExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    ThrowArrayIndexOutOfBoundsException(idx, length);
    SetExceptionEvent(events::ExceptionType::BOUND_CHECK);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void StringIndexOutOfBoundsExceptionEntrypoint([[maybe_unused]] ssize_t idx,
                                                                              [[maybe_unused]] size_t length)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "StringIndexOutOfBoundsExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    ThrowStringIndexOutOfBoundsException(idx, length);
    SetExceptionEvent(events::ExceptionType::BOUND_CHECK);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void NullPointerExceptionEntrypoint()
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "NullPointerExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    ThrowNullPointerException();
    SetExceptionEvent(events::ExceptionType::NULL_CHECK);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void AbstractMethodErrorEntrypoint(Method *method)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "AbstractMethodErrorEntrypoint \n";
    ManagedThread *thread = ManagedThread::GetCurrent();
    ASSERT(!thread->HasPendingException());
    auto stack = StackWalker::Create(thread, UnwindPolicy::SKIP_INLINED);
    ThrowAbstractMethodError(method);
    ASSERT(thread->HasPendingException());
    SetExceptionEvent(events::ExceptionType::ABSTRACT_METHOD);
    if (stack.IsCFrame()) {
        FindCatchBlockInCFrames(thread, &stack, nullptr);
    }
}

extern "C" NO_ADDRESS_SANITIZE void ArithmeticExceptionEntrypoint()
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "ArithmeticExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    ThrowArithmeticException();
    SetExceptionEvent(events::ExceptionType::ARITHMETIC);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void NegativeArraySizeExceptionEntrypoint(ssize_t size)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "NegativeArraySizeExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    ThrowNegativeArraySizeException(size);
    SetExceptionEvent(events::ExceptionType::NEGATIVE_SIZE);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void ClassCastExceptionEntrypoint(Class *instClass, ObjectHeader *srcObj)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "ClassCastExceptionEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    ASSERT(srcObj != nullptr);
    ThrowClassCastException(instClass, srcObj->ClassAddr<Class>());
    SetExceptionEvent(events::ExceptionType::CAST_CHECK);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void StackOverflowExceptionEntrypoint()
{
    // WARNING: We should not add any heavy code constructions here, like events or other debug/testing stuff,
    // because we have small stack here, see ManagedThread::STACK_OVERFLOW_RESERVED_SIZE.
    auto thread = ManagedThread::GetCurrent();

    ASSERT(!thread->HasPendingException());
    thread->DisableStackOverflowCheck();
    ThrowStackOverflowException(thread);
    thread->EnableStackOverflowCheck();
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void DeoptimizeEntrypoint(uint64_t deoptimizeType)
{
    BEGIN_ENTRYPOINT();
    auto thread = ManagedThread::GetCurrent();
    auto type = static_cast<panda::compiler::DeoptimizeType>(
        deoptimizeType & ((1U << MinimumBitsToStore(panda::compiler::DeoptimizeType::COUNT)) - 1));
    [[maybe_unused]] auto instId = deoptimizeType >> MinimumBitsToStore(panda::compiler::DeoptimizeType::COUNT);
    LOG(INFO, INTEROP) << "DeoptimizeEntrypoint (reason: " << panda::compiler::DeoptimizeTypeToString(type)
                       << ", inst_id: " << instId << ")\n";

    EVENT_DEOPTIMIZATION_REASON(std::string(StackWalker::Create(thread).GetMethod()->GetFullName()),
                                panda::compiler::DeoptimizeTypeToString(type));

    ASSERT(!thread->HasPendingException());
    auto stack = StackWalker::Create(thread);
    Method *destroyMethod = nullptr;
    if (type >= panda::compiler::DeoptimizeType::CAUSE_METHOD_DESTRUCTION) {
        // Get pointer to top method
        destroyMethod = StackWalker::Create(thread, UnwindPolicy::SKIP_INLINED).GetMethod();
    }
    Deoptimize(&stack, nullptr, false, destroyMethod);
}

extern "C" NO_ADDRESS_SANITIZE void ThrowInstantiationErrorEntrypoint(Class *klass)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "ThrowInstantiationErrorEntrypoint \n";
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    const auto &name = klass->GetName();
    PandaString pname(name.cbegin(), name.cend());
    ThrowInstantiationError(pname);
    SetExceptionEvent(events::ExceptionType::INSTANTIATION_ERROR);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

coretypes::TaggedValue GetInitialTaggedValue(Method *method)
{
    BEGIN_ENTRYPOINT();
    return Runtime::GetCurrent()->GetLanguageContext(*method).GetInitialTaggedValue();
}

extern "C" void LockObjectEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    panda::intrinsics::ObjectMonitorEnter(obj);
}

extern "C" void LockObjectSlowPathEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    panda::intrinsics::ObjectMonitorEnter(obj);
    if (!ManagedThread::GetCurrent()->HasPendingException()) {
        return;
    }
    LOG(DEBUG, INTEROP) << "ThrowNativeExceptionEntrypoint after LockObject \n";
    SetExceptionEvent(events::ExceptionType::NATIVE);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" void UnlockObjectEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    panda::intrinsics::ObjectMonitorExit(obj);
}

extern "C" void UnlockObjectSlowPathEntrypoint(ObjectHeader *obj)
{
    BEGIN_ENTRYPOINT();
    panda::intrinsics::ObjectMonitorExit(obj);
    if (!ManagedThread::GetCurrent()->HasPendingException()) {
        return;
    }
    LOG(DEBUG, INTEROP) << "ThrowNativeExceptionEntrypoint after UnlockObject \n";
    SetExceptionEvent(events::ExceptionType::NATIVE);
    HandlePendingException(UnwindPolicy::SKIP_INLINED);
}

extern "C" NO_ADDRESS_SANITIZE void IncompatibleClassChangeErrorForMethodConflictEntrypoint(Method *method)
{
    BEGIN_ENTRYPOINT();
    LOG(DEBUG, INTEROP) << "IncompatibleClassChangeErrorForMethodConflictEntrypoint \n";
    ManagedThread *thread = ManagedThread::GetCurrent();
    ASSERT(!thread->HasPendingException());
    auto stack = StackWalker::Create(thread, UnwindPolicy::SKIP_INLINED);
    ThrowIncompatibleClassChangeErrorForMethodConflict(method);
    ASSERT(thread->HasPendingException());
    SetExceptionEvent(events::ExceptionType::ICCE_METHOD_CONFLICT);
    if (stack.IsCFrame()) {
        FindCatchBlockInCFrames(thread, &stack, nullptr);
    }
}

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
extern "C" void TraceEntrypoint(size_t pid, ...)
{
    LOG_ENTRYPOINT();
    auto id = static_cast<TraceId>(pid);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    va_list args;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    va_start(args, pid);
    switch (id) {
        case TraceId::METHOD_ENTER: {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,clang-analyzer-valist.Uninitialized)
            [[maybe_unused]] auto method = va_arg(args, const Method *);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            [[maybe_unused]] auto kind = va_arg(args, events::MethodEnterKind);
            EVENT_METHOD_ENTER(method->GetFullName(), kind, ManagedThread::GetCurrent()->RecordMethodEnter());
            break;
        }
        case TraceId::METHOD_EXIT: {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,clang-analyzer-valist.Uninitialized)
            [[maybe_unused]] auto method = va_arg(args, const Method *);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            [[maybe_unused]] auto kind = va_arg(args, events::MethodExitKind);
            EVENT_METHOD_EXIT(method->GetFullName(), kind, ManagedThread::GetCurrent()->RecordMethodExit());
            break;
        }
        case TraceId::PRINT_ARG: {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,clang-analyzer-valist.Uninitialized)
            size_t argsNum = va_arg(args, size_t);
            std::cerr << "[TRACE ARGS] ";
            for (size_t i = 0; i < argsNum; i++) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                std::cerr << i << "=" << va_arg(args, void *) << " ";
            }
            std::cerr << std::endl;

            break;
        }
        default: {
            break;
        }
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    va_end(args);
}

extern "C" void LogEntrypoint(const char *fmt, ...)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    va_list args;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    vfprintf(stderr, fmt, args);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    va_end(args);
}

// Irtoc Interpreter Entrypoints

extern "C" void FreeFrameInterp(Frame *frame, ManagedThread *current)
{
    ASSERT(frame->GetExt() != nullptr);
    current->GetStackFrameAllocator()->Free(frame->GetExt());
}

extern "C" void *AllocFrameInterp(ManagedThread *current, size_t allocSz)
{
    void *mem = current->GetStackFrameAllocator()->Alloc<false>(allocSz);
    if (UNLIKELY(mem == nullptr)) {
        current->DisableStackOverflowCheck();
        panda::ThrowStackOverflowException(current);
        current->EnableStackOverflowCheck();
    }
    return mem;
}

extern "C" Frame *InitializeFrame(void *mem, Method *method, Frame *prev, size_t nregs)
{
    return new (Frame::FromExt(mem, CORE_EXT_FRAME_DATA_SIZE)) Frame(mem, method, prev, nregs);
}

extern "C" void SafepointEntrypointInterp(ManagedThread *thread)
{
    BEGIN_ENTRYPOINT();
    interpreter::RuntimeInterface::Safepoint(thread);
}

extern "C" int IsCompiled(const void *entrypoint)
{
    return static_cast<int>(entrypoint != reinterpret_cast<const void *>(CompiledCodeToInterpreterBridge));
}

extern "C" size_t GetClassIdEntrypoint(const Method *caller, uint32_t classId)
{
    auto resolvedId = caller->GetClass()->ResolveClassIndex(BytecodeId(classId).AsIndex());
    return resolvedId.GetOffset();
}

extern "C" coretypes::Array *CreateArrayByIdEntrypoint(ManagedThread *thread, const Method *caller, uint32_t classId,
                                                       int32_t length)
{
    CHECK_STACK_WALKER;
    auto *klass = interpreter::RuntimeInterface::ResolveClass<true>(thread, *caller, BytecodeId(classId));
    if (UNLIKELY(klass == nullptr)) {
        return nullptr;
    }
    return interpreter::RuntimeInterface::CreateArray(klass, length);
}

template <BytecodeInstruction::Format FORMAT>
static coretypes::Array *CreateMultiDimArray(ManagedThread *thread, Frame *frame, Class *klass, Method *caller,
                                             uint32_t methodId, const uint8_t *pc)
{
    interpreter::DimIterator<FORMAT> dimIter {BytecodeInstruction(pc), frame};
    auto nargs = interpreter::RuntimeInterface::GetMethodArgumentsCount(caller, BytecodeId(methodId));
    auto obj =
        coretypes::Array::CreateMultiDimensionalArray<interpreter::DimIterator<FORMAT>>(thread, klass, nargs, dimIter);
    return obj;
}

extern "C" coretypes::Array *CreateMultiDimensionalArrayById(ManagedThread *thread, Frame *frame, Class *klass,
                                                             Method *caller, uint32_t methodId, const uint8_t *pc,
                                                             int32_t formatIdx)
{
    switch (formatIdx) {
        case 0U:
            return CreateMultiDimArray<BytecodeInstruction::Format::V4_V4_ID16>(thread, frame, klass, caller, methodId,
                                                                                pc);
        case 1U:
            return CreateMultiDimArray<BytecodeInstruction::Format::V4_V4_V4_V4_ID16>(thread, frame, klass, caller,
                                                                                      methodId, pc);
        case 2U:
            return CreateMultiDimArray<BytecodeInstruction::Format::V8_ID16>(thread, frame, klass, caller, methodId,
                                                                             pc);
        default:
            UNREACHABLE();
    }
    return nullptr;
}

extern "C" ObjectHeader *CreateObjectByClassInterpreter(ManagedThread *thread, Class *klass)
{
    ASSERT(klass != nullptr);
    ASSERT(!klass->IsArrayClass());

    if (!klass->IsInitialized()) {
        auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
        if (!classLinker->InitializeClass(thread, klass)) {
            return nullptr;
        }
    }

    return interpreter::RuntimeInterface::CreateObject(thread, klass);
}

extern "C" uint32_t CheckCastByIdEntrypoint(ObjectHeader *obj, Class *klass)
{
    CHECK_STACK_WALKER;
    ASSERT(IsAddressInObjectsHeapOrNull(obj));
    Class *objKlass = obj == nullptr ? nullptr : obj->ClassAddr<Class>();
    if (UNLIKELY(objKlass != nullptr && !klass->IsAssignableFrom(objKlass))) {
        panda::ThrowClassCastException(klass, objKlass);
        return 1;
    }
    return 0;
}

extern "C" uint32_t IsInstanceByIdEntrypoint(ObjectHeader *obj, Class *klass)
{
    CHECK_STACK_WALKER;
    ASSERT(IsAddressInObjectsHeapOrNull(obj));

    return IsInstanceEntrypoint(obj, klass);
}

extern "C" coretypes::String *ResolveStringByIdEntrypoint(ManagedThread *thread, Frame *frame, FileEntityId id)
{
    BEGIN_ENTRYPOINT();
    return thread->GetVM()->ResolveString(frame, panda_file::File::EntityId(id));
}

extern "C" coretypes::Array *ResolveLiteralArrayByIdEntrypoint(ManagedThread *thread, const Method *caller,
                                                               uint32_t typeId)
{
    BEGIN_ENTRYPOINT();
    return interpreter::RuntimeInterface::ResolveLiteralArray(thread->GetVM(), *caller, BytecodeId(typeId));
}

extern "C" Class *ResolveTypeByIdEntrypoint(ManagedThread *thread, Method *caller, uint32_t id,
                                            InterpreterCache::Entry *entry, const uint8_t *pc)
{
    CHECK_STACK_WALKER;
    auto klass = interpreter::RuntimeInterface::ResolveClass<false>(thread, *caller, BytecodeId(id));
    if (klass == nullptr) {
        return nullptr;
    }
    *entry = {pc, caller, static_cast<void *>(klass)};
    return klass;
}

extern "C" Field *GetFieldByIdEntrypoint([[maybe_unused]] ManagedThread *thread, Method *caller, uint32_t fieldId,
                                         InterpreterCache::Entry *entry, const uint8_t *pc)
{
    CHECK_STACK_WALKER;
    auto resolvedId = caller->GetClass()->ResolveFieldIndex(BytecodeId(fieldId).AsIndex());
    auto *classLinker = Runtime::GetCurrent()->GetClassLinker();
    auto *field = classLinker->GetField(*caller, panda_file::File::EntityId(resolvedId));
    if (field != nullptr) {
        *entry = {pc, caller, static_cast<void *>(field)};
    }
    return field;
}

extern "C" Field *GetStaticFieldByIdEntrypoint(ManagedThread *thread, Method *caller, uint32_t fieldId,
                                               InterpreterCache::Entry *entry, const uint8_t *pc)
{
    CHECK_STACK_WALKER;
    auto *field = interpreter::RuntimeInterface::ResolveField(thread, *caller, BytecodeId(fieldId));
    if (field == nullptr) {
        return field;
    }
    ASSERT(field->IsStatic());
    *entry = {pc, caller, static_cast<void *>(field)};
    return field;
}

extern "C" Method *GetCalleeMethodFromBytecodeId(ManagedThread *thread, Method *caller, uint32_t calleeId,
                                                 InterpreterCache::Entry *entry, const uint8_t *pc)
{
    CHECK_STACK_WALKER;
    auto *method = interpreter::RuntimeInterface::ResolveMethod(thread, *caller, BytecodeId(calleeId));
    if (method == nullptr) {
        return nullptr;  // if nullptr we don't need to cache
    }
    *entry = {pc, caller, static_cast<void *>(method)};
    return method;
}

extern "C" Class *GetMethodClassById(Method *method, uint32_t methodId)
{
    CHECK_STACK_WALKER;
    Class *klass = interpreter::RuntimeInterface::GetMethodClass(method, BytecodeId(methodId));
    return klass;  // May be nullptr if exception occured
}

extern "C" Method *ResolveVirtualMethod(const Method *callee, Frame *frame, const ObjectPointerType objPtr,
                                        const uint8_t *pc, Method *caller)
{
    auto *obj = reinterpret_cast<const ObjectHeader *>(objPtr);
    ASSERT(IsAddressInObjectsHeap(obj));
    auto *cls = obj->ClassAddr<Class>();
    ASSERT(cls != nullptr);
    auto *resolved = cls->ResolveVirtualMethod(callee);
    ASSERT(resolved != nullptr);

    ProfilingData *profData = caller->GetProfilingData();
    auto bytecodeOffset = pc - frame->GetInstruction();
    if (profData != nullptr) {
        profData->UpdateInlineCaches(bytecodeOffset, cls);
    }

    return resolved;
}

extern "C" bool Verify(Method *method)
{
    if (UNLIKELY(!method->Verify())) {
        interpreter::RuntimeInterface::ThrowVerificationException(method->GetFullName());
        return false;
    }
    return true;
}

extern "C" bool DecrementHotnessCounter(Method *method, ManagedThread *thread)
{
    method->DecrementHotnessCounter<true>(thread, 0, nullptr);
    if (thread->HasPendingException()) {
        return false;
    }
    return method->GetCompiledEntryPoint() != GetCompiledCodeToInterpreterBridge(method);
}

extern "C" bool DecrementHotnessCounterDyn(Method *method, TaggedValue funcObj, ManagedThread *thread)
{
    method->DecrementHotnessCounter<true>(thread, 0, nullptr, false, funcObj);
    if (thread->HasPendingException()) {
        return false;
    }
    return method->GetCompiledEntryPoint() != GetCompiledCodeToInterpreterBridge(method);
}

extern "C" void CallCompilerSlowPath(ManagedThread *thread, Method *method)
{
    CHECK_STACK_WALKER;
    method->DecrementHotnessCounter<false>(thread, 0, nullptr);
}

extern "C" bool CallCompilerSlowPathOSR(ManagedThread *thread, Method *method, Frame *frame, uint64_t acc,
                                        uint64_t accTag, uint32_t insOffset, int offset)
{
    CHECK_STACK_WALKER;
    if constexpr (ArchTraits<RUNTIME_ARCH>::SUPPORT_OSR) {
        ASSERT(ArchTraits<RUNTIME_ARCH>::SUPPORT_OSR);
        if (!frame->IsDeoptimized() && Runtime::GetOptions().IsCompilerEnableOsr()) {
            frame->SetAcc(panda::interpreter::AccVRegister(acc, accTag));
            return method->DecrementHotnessCounter<false>(thread, insOffset + offset, &frame->GetAcc(), true);
        }
    }
    method->DecrementHotnessCounter<false>(thread, 0, nullptr);
    return false;
}

extern "C" void UpdateBranchTaken([[maybe_unused]] Method *method, Frame *frame, const uint8_t *pc,
                                  ProfilingData *profDataIrtoc)
{
    // Add a second prof_data loading because without it THREAD_SANITIZER crashes
    // NOTE(aantipina): investigate and delete the second loading (issue I6DTAA)
    [[maybe_unused]] ProfilingData *profData = method->GetProfilingDataWithoutCheck();
    profDataIrtoc->UpdateBranchTaken(pc - frame->GetInstruction());
}

extern "C" void UpdateBranchUntaken([[maybe_unused]] Method *method, Frame *frame, const uint8_t *pc,
                                    ProfilingData *profDataIrtoc)
{
    // Add a second prof_data loading because without it THREAD_SANITIZER crashes
    // NOTE(aantipina): investigate and delete the second loading
    [[maybe_unused]] ProfilingData *profData = method->GetProfilingDataWithoutCheck();
    profDataIrtoc->UpdateBranchNotTaken(pc - frame->GetInstruction());
}

extern "C" uint32_t ReadUlebEntrypoint(const uint8_t *ptr)
{
    uint32_t result;
    [[maybe_unused]] size_t n;
    [[maybe_unused]] bool isFull;
    std::tie(result, n, isFull) = leb128::DecodeUnsigned<uint32_t>(ptr);
    ASSERT(isFull);
    return result;
}

extern "C" const uint8_t *GetInstructionsByMethod(const Method *method)
{
    return method->GetInstructions();
}

extern "C" size_t GetNumVregsByMethod(const Method *method)
{
    return method->GetNumVregs();
}

extern "C" void ThrowExceptionFromInterpreter(ManagedThread *thread, ObjectHeader *exception, Frame *frame,
                                              const uint8_t *pc)
{
    CHECK_STACK_WALKER;
    ASSERT(!thread->HasPendingException());
    ASSERT(IsAddressInObjectsHeap(exception));
    Method *method = frame->GetMethod();
    ProfilingData *profData = method->GetProfilingDataWithoutCheck();
    if (profData != nullptr) {
        profData->UpdateThrowTaken(pc - frame->GetInstruction());
    }
    thread->SetException(exception);
}

extern "C" void ThrowArithmeticExceptionFromInterpreter()
{
    CHECK_STACK_WALKER;
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    interpreter::RuntimeInterface::ThrowArithmeticException();
}

extern "C" void ThrowNullPointerExceptionFromInterpreter()
{
    CHECK_STACK_WALKER;
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    interpreter::RuntimeInterface::ThrowNullPointerException();
}

extern "C" void ThrowNegativeArraySizeExceptionFromInterpreter(int32_t size)
{
    CHECK_STACK_WALKER;
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    interpreter::RuntimeInterface::ThrowNegativeArraySizeException(size);
}

extern "C" void ThrowArrayIndexOutOfBoundsExceptionFromInterpreter(ssize_t idx, size_t length)
{
    CHECK_STACK_WALKER;
    ASSERT(!ManagedThread::GetCurrent()->HasPendingException());
    interpreter::RuntimeInterface::ThrowArrayIndexOutOfBoundsException(idx, length);
}

extern "C" uint8_t CheckStoreArrayReferenceFromInterpreter(coretypes::Array *array, ObjectHeader *storeObj)
{
    CHECK_STACK_WALKER;
    ASSERT(array != nullptr);
    ASSERT(IsAddressInObjectsHeapOrNull(storeObj));

    if (storeObj == nullptr) {  // NOTE: may be moved to IRTOC
        return 0;
    }

    // SUPPRESS_CSA_NEXTLINE(alpha.core.WasteObjHeader)
    auto *arrayClass = array->ClassAddr<Class>();
    auto *elementClass = arrayClass->GetComponentType();
    // SUPPRESS_CSA_NEXTLINE(alpha.core.WasteObjHeader)
    if (!storeObj->IsInstanceOf(elementClass)) {
        // SUPPRESS_CSA_NEXTLINE(alpha.core.WasteObjHeader)
        interpreter::RuntimeInterface::ThrowArrayStoreException(arrayClass, storeObj->ClassAddr<Class>());
        return 1;
    }
    return 0;
}

extern "C" uint32_t FindCatchBlockInIFramesStackless(ManagedThread **currThread, Frame **currFrame, const uint8_t *pc)
{
    uint32_t inst = pc - (*currFrame)->GetInstruction();
    Frame *frame = *currFrame;

    while (frame != nullptr) {
        ManagedThread *thread = *currThread;
        Frame *prev = frame->GetPrevFrame();
        ASSERT(thread->HasPendingException());

        Method *method = frame->GetMethod();
        uint32_t pcOffset = interpreter::RuntimeInterface::FindCatchBlock(*method, thread->GetException(), inst);
        if (pcOffset != panda_file::INVALID_OFFSET) {
            return pcOffset;
        }

        if (!frame->IsStackless() || prev == nullptr || StackWalker::IsBoundaryFrame<FrameKind::INTERPRETER>(prev)) {
            return pcOffset;
        }

        EVENT_METHOD_EXIT(method->GetFullName(), events::MethodExitKind::INTERP, thread->RecordMethodExit());

        Runtime::GetCurrent()->GetNotificationManager()->MethodExitEvent(thread, method);

        inst = prev->GetBytecodeOffset();
        *currFrame = prev;

        thread->GetVM()->HandleReturnFrame();

        interpreter::RuntimeInterface::SetCurrentFrame(thread, prev);

        ASSERT(thread->HasPendingException());

        interpreter::RuntimeInterface::FreeFrame(*currThread, frame);

        LOG(DEBUG, INTERPRETER) << "Exit: Runtime Call.";

        frame = prev;
    }

    return panda_file::INVALID_OFFSET;
}

extern "C" const uint8_t *FindCatchBlockInIFrames(ManagedThread *currThread, Frame *currFrame, const uint8_t *pc)
{
    CHECK_STACK_WALKER;

    uint32_t pcOffset = panda_file::INVALID_OFFSET;

    pcOffset = FindCatchBlockInIFramesStackless(&currThread, &currFrame, pc);

    if (pcOffset == panda_file::INVALID_OFFSET) {
        if constexpr (RUNTIME_ARCH == Arch::AARCH64 || RUNTIME_ARCH == Arch::AARCH32 || RUNTIME_ARCH == Arch::X86_64) {
            panda::FindCatchBlockInCallStack(currThread);
        }
        return pc;
    }

    Method *method = currFrame->GetMethod();
    ASSERT(method != nullptr);
    LanguageContext ctx = interpreter::RuntimeInterface::GetLanguageContext(*method);
    ObjectHeader *exceptionObject = currThread->GetException();
    ctx.SetExceptionToVReg(currFrame->GetAcc(), exceptionObject);

    currThread->ClearException();
    Span<const uint8_t> sp(currFrame->GetMethod()->GetInstructions(), pcOffset);
    return sp.cend();
}

extern "C" coretypes::String *VmCreateString(ManagedThread *thread, Method *method, ObjectHeader *ctorArg)
{
    CHECK_STACK_WALKER;
    // If ctor has the only argument (object itself), ctor_arg is allowed to contain garbage
    ASSERT(method->GetNumArgs() == 1 || IsAddressInObjectsHeapOrNull(ctorArg));

    return thread->GetVM()->CreateString(method, ctorArg);
}

extern "C" void DebugPrintEntrypoint([[maybe_unused]] panda::Frame *frame, [[maybe_unused]] const uint8_t *pc,
                                     [[maybe_unused]] uint64_t accPayload, [[maybe_unused]] uint64_t accTag)
{
    CHECK_STACK_WALKER;
#ifndef NDEBUG
    if (!Logger::IsLoggingOn(Logger::Level::DEBUG, Logger::Component::INTERPRETER)) {
        return;
    }

    constexpr uint64_t STANDARD_DEBUG_INDENT = 5;
    PandaString accDump;
    auto accVreg = panda::interpreter::VRegister(accPayload);
    auto accTagVreg = panda::interpreter::VRegister(accTag);
    if (frame->IsDynamic()) {
        accDump = panda::interpreter::DynamicVRegisterRef(&accVreg).DumpVReg();
        LOG(DEBUG, INTERPRETER) << PandaString(STANDARD_DEBUG_INDENT, ' ') << "acc." << accDump;

        DynamicFrameHandler frameHandler(frame);
        for (size_t i = 0; i < frame->GetSize(); ++i) {
            LOG(DEBUG, INTERPRETER) << PandaString(STANDARD_DEBUG_INDENT, ' ') << "v" << i << "."
                                    << frameHandler.GetVReg(i).DumpVReg();
        }
    } else {
        accDump = panda::interpreter::StaticVRegisterRef(&accVreg, &accTagVreg).DumpVReg();
        LOG(DEBUG, INTERPRETER) << PandaString(STANDARD_DEBUG_INDENT, ' ') << "acc." << accDump;

        StaticFrameHandler frameHandler(frame);
        for (size_t i = 0; i < frame->GetSize(); ++i) {
            LOG(DEBUG, INTERPRETER) << PandaString(STANDARD_DEBUG_INDENT, ' ') << "v" << i << "."
                                    << frameHandler.GetVReg(i).DumpVReg();
        }
    }
    LOG(DEBUG, INTERPRETER) << " pc: " << (void *)pc << " ---> " << BytecodeInstruction(pc);
#endif
}

extern "C" PANDA_PUBLIC_API int32_t JsCastDoubleToInt32(double d)
{
    return coretypes::JsCastDoubleToInt(d, coretypes::INT32_BITS);
}

extern "C" int32_t JsCastDoubleToInt32Entrypoint(uint64_t d)
{
    return coretypes::JsCastDoubleToInt(bit_cast<double>(d), coretypes::INT32_BITS);
}

/* The shared implementation of the StringBuilder.append() intrinsics */
template <class T>
static T *ToClassPtr(uintptr_t obj)
{
    return reinterpret_cast<T *>(*reinterpret_cast<ObjectPointerType *>(obj));
}

/* offset values to be aligned with the StringBuilder class layout */
static constexpr uint32_t SB_COUNT_OFFSET = 12;
static constexpr uint32_t SB_VALUE_OFFSET = 8;

static uint32_t GetCountValue(ObjectHeader *sb)
{
    /* StringBuilder is not thread safe, we are supposed to
     * provide just the regular access to it's fields */
    return sb->GetFieldPrimitive<uint32_t>(SB_COUNT_OFFSET);
}

static uint32_t GetCapacity(ObjectHeader *self)
{
    return ToClassPtr<coretypes::Array>(ToUintPtr(self) + SB_VALUE_OFFSET)->GetLength();
}

static uint16_t *GetStorageAddress(ObjectHeader *sb, uint32_t count)
{
    auto storage = ToClassPtr<coretypes::Array>(ToUintPtr(sb) + SB_VALUE_OFFSET);
    return reinterpret_cast<uint16_t *>(ToUintPtr(storage) + coretypes::Array::GetDataOffset() + (count << 1U));
}

static inline uint32_t RecalculateCapacity(uint32_t capacity, uint32_t newsize)
{
    capacity = (capacity + 1) << 1U;
    capacity = newsize > capacity ? newsize : capacity;
    return capacity;
}

static void MoveStorageData(ObjectHeader *sb, void *newstorage)
{
    auto size = GetCountValue(sb) << 1U;
    auto newbuf = ToVoidPtr(ToUintPtr(newstorage) + coretypes::Array::GetDataOffset());
    auto oldbuf = ToVoidPtr(ToUintPtr(GetStorageAddress(sb, 0U)));
    memcpy(newbuf, oldbuf, size);
}

static std::tuple<bool, ObjectHeader *, coretypes::String *> AssureCapacity(ObjectHeader *sb, uint32_t newsize,
                                                                            coretypes::String *str)
{
    if (GetCapacity(sb) >= newsize) {
        return std::make_tuple(true, sb, str);
    }

    auto thread = ManagedThread::GetCurrent();
    HandleScope<ObjectHeader *> scope(thread);
    VMHandle<ObjectHeader> handle(thread, sb);
    VMHandle<coretypes::String> strHandle {};
    strHandle = str != nullptr ? VMHandle<coretypes::String>(thread, str) : strHandle;

    auto *vm = Runtime::GetCurrent()->GetPandaVM();
    auto klass = Runtime::GetCurrent()
                     ->GetClassLinker()
                     ->GetExtension(vm->GetLanguageContext())
                     ->GetClassRoot(ClassRoot::ARRAY_U16);

    auto capacity = RecalculateCapacity(GetCapacity(sb), newsize);
    auto *newstorage = panda::coretypes::Array::Create(klass, capacity);
    sb = reinterpret_cast<ObjectHeader *>(handle.GetPtr());
    str = str != nullptr ? strHandle.GetPtr() : nullptr;

    if (newstorage == nullptr) {
        return std::make_tuple(false, sb, str);
    }

    MoveStorageData(sb, newstorage);
    handle->SetFieldObject(SB_VALUE_OFFSET, newstorage);

    return std::make_tuple(true, sb, str);
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define check_capacity_fast(sb, newsize)                                           \
    {                                                                              \
        auto success = false;                                                      \
        std::tie(success, sb, std::ignore) = AssureCapacity(sb, newsize, nullptr); \
                                                                                   \
        if (!success) {                                                            \
            return sb;                                                             \
        }                                                                          \
    }

static constexpr auto TPOWER1 = 10U;
static constexpr auto TPOWER2 = 100;
static constexpr auto TPOWER3 = 1000;
static constexpr auto TPOWER4 = 10000;

static uint32_t CountDigits(uint64_t n)
{
    uint32_t count = 1;
    while (true) {
        if (n < TPOWER1) {
            return count;
        }
        if (n < TPOWER2) {
            return count + 1;
        }
        if (n < TPOWER3) {
            return count + 2;
        }
        if (n < TPOWER4) {
            return count + 3;
        }
        count += 4U;
        n /= TPOWER4;
    }
    return count;
}

static uint32_t constexpr MAX_BIDIGITS = 100U;
static constexpr uint32_t BIDIGITS[MAX_BIDIGITS] = {  // NOLINT(modernize-avoid-c-arrays)
    0x00300030, 0x00310030, 0x00320030, 0x00330030, 0x00340030, 0x00350030, 0x00360030, 0x00370030, 0x00380030,
    0x00390030, 0x00300031, 0x00310031, 0x00320031, 0x00330031, 0x00340031, 0x00350031, 0x00360031, 0x00370031,
    0x00380031, 0x00390031, 0x00300032, 0x00310032, 0x00320032, 0x00330032, 0x00340032, 0x00350032, 0x00360032,
    0x00370032, 0x00380032, 0x00390032, 0x00300033, 0x00310033, 0x00320033, 0x00330033, 0x00340033, 0x00350033,
    0x00360033, 0x00370033, 0x00380033, 0x00390033, 0x00300034, 0x00310034, 0x00320034, 0x00330034, 0x00340034,
    0x00350034, 0x00360034, 0x00370034, 0x00380034, 0x00390034, 0x00300035, 0x00310035, 0x00320035, 0x00330035,
    0x00340035, 0x00350035, 0x00360035, 0x00370035, 0x00380035, 0x00390035, 0x00300036, 0x00310036, 0x00320036,
    0x00330036, 0x00340036, 0x00350036, 0x00360036, 0x00370036, 0x00380036, 0x00390036, 0x00300037, 0x00310037,
    0x00320037, 0x00330037, 0x00340037, 0x00350037, 0x00360037, 0x00370037, 0x00380037, 0x00390037, 0x00300038,
    0x00310038, 0x00320038, 0x00330038, 0x00340038, 0x00350038, 0x00360038, 0x00370038, 0x00380038, 0x00390038,
    0x00300039, 0x00310039, 0x00320039, 0x00330039, 0x00340039, 0x00350039, 0x00360039, 0x00370039, 0x00380039,
    0x00390039};

static ObjectHeader *StoreNumber(ObjectHeader *sb, int64_t n)
{
    auto num = n < 0 ? -static_cast<uint64_t>(n) : n;
    auto size = CountDigits(num) + static_cast<uint32_t>(n < 0);
    auto count = GetCountValue(sb);
    auto newsize = count + size;

    check_capacity_fast(sb, newsize);
    auto dst = reinterpret_cast<uint32_t *>(GetStorageAddress(sb, newsize));

    while (num >= TPOWER2) {
        *(--dst) = BIDIGITS[num % TPOWER2];
        num /= TPOWER2;
    }
    if (num < TPOWER1) {
        auto dst1 = reinterpret_cast<uint16_t *>(dst);
        *(--dst1) = (num + '0');
    } else {
        *(--dst) = BIDIGITS[num];
    }
    if (n < 0) {
        *GetStorageAddress(sb, count) = '-';
    }

    sb->SetFieldPrimitive<uint32_t>(SB_COUNT_OFFSET, newsize);
    return sb;
}

extern "C" ObjectHeader *CoreStringBuilderLong(ObjectHeader *sb, int64_t n)
{
    return StoreNumber(sb, n);
}

extern "C" ObjectHeader *CoreStringBuilderInt(ObjectHeader *sb, int32_t n)
{
    return StoreNumber(sb, static_cast<int64_t>(n));
}

extern "C" ObjectHeader *CoreStringBuilderBool(ObjectHeader *sb, const uint8_t v)
{
    const uint64_t truestr = 0x0065007500720074;
    const uint64_t falsestr = 0x0073006c00610066;
    const uint16_t falsestr2 = 0x0065;

    auto count = GetCountValue(sb);
    auto size = 4U + (1U - v);  // v is actually u1
    auto newsize = count + size;

    check_capacity_fast(sb, newsize);
    auto dst = reinterpret_cast<uint64_t *>(GetStorageAddress(sb, count));

    if (v != 0) {
        *dst = truestr;
    } else {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto dst2 = reinterpret_cast<uint16_t *>(dst + 1);
        *dst = falsestr;
        *dst2 = falsestr2;
    }

    sb->SetFieldPrimitive<uint32_t>(SB_COUNT_OFFSET, newsize);
    return sb;
}

extern "C" ObjectHeader *CoreStringBuilderChar(ObjectHeader *sb, const uint16_t c)
{
    auto count = GetCountValue(sb);
    auto newsize = count + 1;

    check_capacity_fast(sb, newsize);
    auto dst = GetStorageAddress(sb, count);

    *dst = c;
    sb->SetFieldPrimitive<uint32_t>(SB_COUNT_OFFSET, newsize);
    return sb;
}

extern "C" ObjectHeader *CoreStringBuilderString(ObjectHeader *sb, void *s)
{
    const uint64_t nullstr = 0x006c006c0075006e;
    auto str = static_cast<coretypes::String *>(s);
    auto isnull = str == nullptr;
    auto size = isnull ? sizeof(nullstr) / sizeof(uint16_t) : str->GetLength();

    if (size == 0) {
        return sb;
    }

    auto count = GetCountValue(sb);
    auto newsize = count + size;

    auto success = false;
    std::tie(success, sb, str) = AssureCapacity(sb, newsize, str);

    if (!success) {
        return sb;
    }

    auto dst = GetStorageAddress(sb, count);

    if (!isnull) {
        auto src = ToVoidPtr(ToUintPtr(str) + coretypes::String::GetDataOffset());
        str->IsUtf16() ? std::copy_n(reinterpret_cast<uint16_t *>(src), size, dst)
                       : std::copy_n(reinterpret_cast<uint8_t *>(src), size, dst);
    } else {
        /* store the 8-bytes chunk right away */
        *reinterpret_cast<uint64_t *>(dst) = nullstr;
    }

    sb->SetFieldPrimitive<uint32_t>(SB_COUNT_OFFSET, newsize);
    return sb;
}
}  // namespace panda
