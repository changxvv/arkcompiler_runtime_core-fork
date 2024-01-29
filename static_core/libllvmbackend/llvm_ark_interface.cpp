/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "compiler/optimizer/ir/runtime_interface.h"
#include "compiler/optimizer/ir/aot_data.h"
#include "runtime/include/method.h"
#include "events/events.h"
#include "compiler_options.h"
#include "utils/cframe_layout.h"
#include "llvm_ark_interface.h"
#include "llvm_logger.h"

#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include "llvm_options.h"

#include "aot/aot_builder/aot_builder.h"

using panda::compiler::AotBuilder;
using panda::compiler::AotData;
using panda::compiler::RuntimeInterface;

static constexpr auto MEMCPY_BIG_SIZE_FOR_X86_64_SSE = 128;
static constexpr auto MEMCPY_BIG_SIZE_FOR_X86_64_NO_SSE = 64;
static constexpr auto MEMCPY_BIG_SIZE_FOR_AARCH64 = 1024;

static constexpr auto MEMSET_SMALL_SIZE_FOR_AARCH64 = 1024;
static constexpr auto MEMSET_SMALL_SIZE_FOR_X86_64_SSE = 512;
static constexpr auto MEMSET_SMALL_SIZE_FOR_X86_64_NO_SSE = 256;

static constexpr auto MEMMOVE_SMALL_SIZE_FOR_AARCH64 = 30;
static constexpr auto MEMMOVE_BIG_SIZE_FOR_AARCH64 = 64;
static constexpr auto MEMMOVE_SMALL_SIZE_FOR_X86_64_SSE = 94;
static constexpr auto MEMMOVE_BIG_SIZE_FOR_X86_64_SSE = 128;
static constexpr auto MEMMOVE_SMALL_SIZE_FOR_X86_64_NO_SSE = 48;
static constexpr auto MEMMOVE_BIG_SIZE_FOR_X86_64_NO_SSE = 64;

static constexpr auto X86_THREAD_REG = 15;
static constexpr auto AARCH64_THREAD_REG = 28U;

namespace {
constexpr panda::Arch LLVMArchToArkArch(llvm::Triple::ArchType arch)
{
    switch (arch) {
        case llvm::Triple::ArchType::aarch64:
            return panda::Arch::AARCH64;
        case llvm::Triple::ArchType::x86_64:
            return panda::Arch::X86_64;
        case llvm::Triple::ArchType::x86:
            return panda::Arch::X86;
        case llvm::Triple::ArchType::arm:
            return panda::Arch::AARCH32;
        default:
            UNREACHABLE();
            return panda::Arch::NONE;
    }
}
#include <entrypoints_gen.inl>
#include <intrinsics_gen.inl>
#include <intrinsic_names_gen.inl>
#include <entrypoints_llvm_ark_interface_gen.inl>
}  // namespace

namespace panda::llvmbackend {

bool LLVMArkInterface::DeoptsEnabled()
{
    return g_options.IsLlvmDeopts();
}

AotData GetAotDataFromBuilder(const class panda::panda_file::File *file, AotBuilder *aotBuilder)
{
    return AotData(file, nullptr, 0, aotBuilder->GetIntfInlineCacheIndex(), aotBuilder->GetGotPlt(),
                   aotBuilder->GetGotVirtIndexes(), aotBuilder->GetGotClass(), aotBuilder->GetGotString(),
                   aotBuilder->GetGotIntfInlineCache(), aotBuilder->GetGotCommon(), nullptr);
}

panda::llvmbackend::LLVMArkInterface::LLVMArkInterface(RuntimeInterface *runtime, llvm::Triple triple,
                                                       AotBuilder *aotBuilder)
    : runtime_(runtime), triple_(std::move(triple)), aotBuilder_(aotBuilder)
{
    ASSERT(runtime != nullptr);
}

intptr_t LLVMArkInterface::GetCompiledEntryPointOffset() const
{
    return runtime_->GetCompiledEntryPointOffset(LLVMArchToArkArch(triple_.getArch()));
}

const char *LLVMArkInterface::GetThreadRegister() const
{
    switch (LLVMArchToArkArch(triple_.getArch())) {
        case Arch::AARCH64: {
            static_assert(ArchTraits<Arch::AARCH64>::THREAD_REG == AARCH64_THREAD_REG);
            return "x28";
        }
        case Arch::X86_64: {
            static_assert(ArchTraits<Arch::X86_64>::THREAD_REG == X86_THREAD_REG);
            return "r15";
        }
        default:
            UNREACHABLE();
    }
}

llvm::Value *LLVMArkInterface::GetOffsetForIntfInlineCache(llvm::CallInst *callInst) const
{
    auto func = callInst->getFunction();
    auto builder = llvm::IRBuilder<>(callInst);
    auto slotId = GetIntfInlineCacheId(func);
    auto aotGot = func->getParent()->getGlobalVariable("__aot_got");
    auto arrayType = llvm::ArrayType::get(builder.getInt64Ty(), 0);
    auto offset = builder.CreateConstInBoundsGEP2_64(arrayType, aotGot, 0, slotId);
    return offset;
}

uint64_t LLVMArkInterface::GetIntfInlineCacheId(const llvm::Function *caller) const
{
    auto callerOriginFile = functionOrigins_.lookup(caller);
    ASSERT_PRINT(callerOriginFile != nullptr,
                 std::string("No origin for function = '") + caller->getName().str() +
                     "'. Use RememberFunctionOrigin to store the origin before calling GetIntfInlineCacheId");
    auto aotData = GetAotDataFromBuilder(callerOriginFile, aotBuilder_);
    uint64_t intfInlineCacheIndex = aotData.GetIntfInlineCacheIndex();
    return aotData.GetIntfInlineCacheId(intfInlineCacheIndex);
}

void LLVMArkInterface::IncrementIntfInlineCacheIndex() const
{
    uint64_t *index = aotBuilder_->GetIntfInlineCacheIndex();
    *index = *index + 1;
}

const char *LLVMArkInterface::GetFramePointerRegister() const
{
    switch (LLVMArchToArkArch(triple_.getArch())) {
        case Arch::AARCH64:
            return "x29";
        case Arch::X86_64:
            return "rbp";
        default:
            UNREACHABLE();
    }
}

void LLVMArkInterface::PutCalleeSavedRegistersMask(const llvm::Function *method, RegMasks masks)
{
    calleeSavedRegisters_.insert({method, masks});
}

LLVMArkInterface::RegMasks LLVMArkInterface::GetCalleeSavedRegistersMask(const llvm::Function *method)
{
    return calleeSavedRegisters_.lookup(method);
}

uintptr_t LLVMArkInterface::GetEntrypointTlsOffset(EntrypointId id) const
{
    Arch arkArch = LLVMArchToArkArch(triple_.getArch());
    using PandaEntrypointId = panda::compiler::RuntimeInterface::EntrypointId;
    return runtime_->GetEntrypointTlsOffset(arkArch, static_cast<PandaEntrypointId>(id));
}

size_t LLVMArkInterface::GetTlsFrameKindOffset() const
{
    return runtime_->GetTlsFrameKindOffset(LLVMArchToArkArch(triple_.getArch()));
}

size_t LLVMArkInterface::GetTlsPreWrbEntrypointOffset() const
{
    Arch arkArch = LLVMArchToArkArch(triple_.getArch());
    return runtime_->GetTlsPreWrbEntrypointOffset(arkArch);
}

uint32_t LLVMArkInterface::GetClassOffset() const
{
    Arch arkArch = LLVMArchToArkArch(triple_.getArch());
    return runtime_->GetClassOffset(arkArch);
}

uint32_t LLVMArkInterface::GetManagedThreadPostWrbOneObjectOffset() const
{
    Arch arkArch = LLVMArchToArkArch(triple_.getArch());
    return cross_values::GetManagedThreadPostWrbOneObjectOffset(arkArch);
}

size_t LLVMArkInterface::GetStackOverflowCheckOffset() const
{
    return runtime_->GetStackOverflowCheckOffset();
}

std::string GetValueAsString(llvm::Value *value)
{
    std::string name;
    auto stream = llvm::raw_string_ostream(name);
    value->print(stream);
    return stream.str();
}

static bool MustLowerMemSet(const llvm::IntrinsicInst *inst, llvm::Triple::ArchType arch)
{
    ASSERT(inst->getIntrinsicID() == llvm::Intrinsic::memset ||
           inst->getIntrinsicID() == llvm::Intrinsic::memset_inline);

    auto sizeArg = inst->getArgOperand(2U);
    if (!llvm::isa<llvm::ConstantInt>(sizeArg)) {
        return true;
    }

    auto arraySize = llvm::cast<llvm::ConstantInt>(sizeArg)->getZExtValue();
    size_t smallSize;

    if (arch == llvm::Triple::ArchType::aarch64) {
        smallSize = MEMSET_SMALL_SIZE_FOR_AARCH64;
    } else {
        ASSERT(arch == llvm::Triple::ArchType::x86_64);
        bool sse42 = panda::compiler::g_options.IsCpuFeatureEnabled(panda::compiler::SSE42);
        smallSize = sse42 ? MEMSET_SMALL_SIZE_FOR_X86_64_SSE : MEMSET_SMALL_SIZE_FOR_X86_64_NO_SSE;
    }
    return arraySize > smallSize;
}

static bool MustLowerMemCpy(const llvm::IntrinsicInst *inst, llvm::Triple::ArchType arch)
{
    ASSERT(inst->getIntrinsicID() == llvm::Intrinsic::memcpy ||
           inst->getIntrinsicID() == llvm::Intrinsic::memcpy_inline);

    auto sizeArg = inst->getArgOperand(2U);
    if (!llvm::isa<llvm::ConstantInt>(sizeArg)) {
        return true;
    }

    auto arraySize = llvm::cast<llvm::ConstantInt>(sizeArg)->getZExtValue();
    if (arch == llvm::Triple::ArchType::x86_64) {
        bool sse42 = panda::compiler::g_options.IsCpuFeatureEnabled(panda::compiler::SSE42);
        if (sse42) {
            return arraySize > MEMCPY_BIG_SIZE_FOR_X86_64_SSE;
        }
        return arraySize > MEMCPY_BIG_SIZE_FOR_X86_64_NO_SSE;
    }

    ASSERT(arch == llvm::Triple::ArchType::aarch64);

    return arraySize > MEMCPY_BIG_SIZE_FOR_AARCH64;
}

static bool MustLowerMemMove(const llvm::IntrinsicInst *inst, llvm::Triple::ArchType arch)
{
    ASSERT(inst->getIntrinsicID() == llvm::Intrinsic::memmove);

    auto sizeArg = inst->getArgOperand(2U);
    if (!llvm::isa<llvm::ConstantInt>(sizeArg)) {
        return true;
    }

    auto arraySize = llvm::cast<llvm::ConstantInt>(sizeArg)->getZExtValue();

    size_t bigSize;
    size_t smallSize;
    int maxPopcount;
    if (arch == llvm::Triple::ArchType::aarch64) {
        smallSize = MEMMOVE_SMALL_SIZE_FOR_AARCH64;
        bigSize = MEMMOVE_BIG_SIZE_FOR_AARCH64;
        maxPopcount = 3U;
    } else {
        ASSERT(arch == llvm::Triple::ArchType::x86_64);
        if (panda::compiler::g_options.IsCpuFeatureEnabled(panda::compiler::SSE42)) {
            smallSize = MEMMOVE_SMALL_SIZE_FOR_X86_64_SSE;
            bigSize = MEMMOVE_BIG_SIZE_FOR_X86_64_SSE;
        } else {
            smallSize = MEMMOVE_SMALL_SIZE_FOR_X86_64_NO_SSE;
            bigSize = MEMMOVE_BIG_SIZE_FOR_X86_64_NO_SSE;
        }
        maxPopcount = 4U;
    }
    if (arraySize <= smallSize) {
        return false;
    }
    if (arraySize > bigSize) {
        return true;
    }
    return Popcount(arraySize) > maxPopcount;
}

llvm::Intrinsic::ID LLVMArkInterface::GetLLVMIntrinsicId(const llvm::Instruction *inst) const
{
    ASSERT(inst != nullptr);

    auto intrinsicInst = llvm::dyn_cast<llvm::IntrinsicInst>(inst);
    if (intrinsicInst == nullptr) {
        return llvm::Intrinsic::not_intrinsic;
    }

    auto llvmId = intrinsicInst->getIntrinsicID();
    if (llvmId == llvm::Intrinsic::memcpy && !MustLowerMemCpy(intrinsicInst, triple_.getArch())) {
        return llvm::Intrinsic::memcpy_inline;
    }
    if (llvmId == llvm::Intrinsic::memset && !MustLowerMemSet(intrinsicInst, triple_.getArch())) {
        return llvm::Intrinsic::memset_inline;
    }

    return llvm::Intrinsic::not_intrinsic;
}

[[maybe_unused]] static bool X86NoSSE(llvm::Triple::ArchType arch)
{
    return arch == llvm::Triple::ArchType::x86_64 &&
           !panda::compiler::g_options.IsCpuFeatureEnabled(panda::compiler::SSE42);
}

#include "get_intrinsic_id_llvm_ark_interface_gen.inl"

LLVMArkInterface::IntrinsicId LLVMArkInterface::GetIntrinsicId(const llvm::Instruction *inst) const
{
    ASSERT(inst != nullptr);
    auto opcode = inst->getOpcode();
    auto type = inst->getType();

    if (opcode == llvm::Instruction::FRem) {
        ASSERT(inst->getNumOperands() == 2U);
        ASSERT(type->isFloatTy() || type->isDoubleTy());
        auto id = type->isFloatTy() ? RuntimeInterface::IntrinsicId::LIB_CALL_FMODF
                                    : RuntimeInterface::IntrinsicId::LIB_CALL_FMOD;
        return static_cast<IntrinsicId>(id);
    }

    auto pluginResult = GetPluginIntrinsicId(inst);
    if (pluginResult != NO_INTRINSIC_ID_CONTINUE) {
        return pluginResult;
    }

    if (const auto *call = llvm::dyn_cast<llvm::CallInst>(inst)) {
        if (const llvm::Function *func = call->getCalledFunction()) {
            // Can be generated in instcombine for `pow` intrinsic
            if (func->getName() == "ldexp" || func->getName() == "ldexpf") {
                ASSERT(type->isDoubleTy() || type->isFloatTy());
                EVENT_PAOC("Lowering @ldexp");
                auto id = type->isDoubleTy() ? RuntimeInterface::IntrinsicId::LIB_CALL_LDEXP
                                             : RuntimeInterface::IntrinsicId::LIB_CALL_LDEXPF;
                return static_cast<IntrinsicId>(id);
            }
        }
    }

    if (llvm::isa<llvm::IntrinsicInst>(inst)) {
        return GetIntrinsicIdSwitch(llvm::cast<llvm::IntrinsicInst>(inst));
    }
    return NO_INTRINSIC_ID;
}

LLVMArkInterface::IntrinsicId LLVMArkInterface::GetIntrinsicIdSwitch(const llvm::IntrinsicInst *inst) const
{
    switch (inst->getIntrinsicID()) {
        case llvm::Intrinsic::memcpy:
        case llvm::Intrinsic::memcpy_inline:
        case llvm::Intrinsic::memmove:
        case llvm::Intrinsic::memset:
        case llvm::Intrinsic::memset_inline:
            return GetIntrinsicIdMemory(inst);
        case llvm::Intrinsic::minimum:
        case llvm::Intrinsic::maximum:
        case llvm::Intrinsic::sin:
        case llvm::Intrinsic::cos:
        case llvm::Intrinsic::pow:
        case llvm::Intrinsic::exp2:
            return GetIntrinsicIdMath(inst);
        case llvm::Intrinsic::ceil:
        case llvm::Intrinsic::floor:
        case llvm::Intrinsic::rint:
        case llvm::Intrinsic::round:
        case llvm::Intrinsic::lround:
        case llvm::Intrinsic::exp:
        case llvm::Intrinsic::log:
        case llvm::Intrinsic::log2:
        case llvm::Intrinsic::log10:
            UNREACHABLE();
        default:
            return NO_INTRINSIC_ID;
    }
    UNREACHABLE();
    return NO_INTRINSIC_ID;
}

LLVMArkInterface::IntrinsicId LLVMArkInterface::GetIntrinsicIdMemory(const llvm::IntrinsicInst *inst) const
{
    auto arch = triple_.getArch();
    auto llvmId = inst->getIntrinsicID();
    switch (llvmId) {
        case llvm::Intrinsic::memcpy:
        case llvm::Intrinsic::memcpy_inline:
            if (!panda::compiler::g_options.IsCompilerNonOptimizing() && !MustLowerMemCpy(inst, arch)) {
                ASSERT(llvmId != llvm::Intrinsic::memcpy);
                EVENT_PAOC("Skip lowering for @llvm.memcpy");
                return NO_INTRINSIC_ID;
            }
            EVENT_PAOC("Lowering @llvm.memcpy");
            return static_cast<IntrinsicId>(RuntimeInterface::IntrinsicId::LIB_CALL_MEM_COPY);
        case llvm::Intrinsic::memmove:
            if (!panda::compiler::g_options.IsCompilerNonOptimizing() && !MustLowerMemMove(inst, arch)) {
                EVENT_PAOC("Skip lowering for @llvm.memmove, size is: " + GetValueAsString(inst->getArgOperand(2U)));
                return NO_INTRINSIC_ID;
            }
            EVENT_PAOC("Lowering @llvm.memmove, size is: " + GetValueAsString(inst->getArgOperand(2U)));
            return static_cast<IntrinsicId>(RuntimeInterface::IntrinsicId::LIB_CALL_MEM_MOVE);
        case llvm::Intrinsic::memset:
        case llvm::Intrinsic::memset_inline:
            if (!panda::compiler::g_options.IsCompilerNonOptimizing() && !MustLowerMemSet(inst, arch)) {
                ASSERT(llvmId != llvm::Intrinsic::memset);
                EVENT_PAOC("Skip lowering for @llvm.memset");
                return NO_INTRINSIC_ID;
            }
            EVENT_PAOC("Lowering @llvm.memset");
            return static_cast<IntrinsicId>(RuntimeInterface::IntrinsicId::LIB_CALL_MEM_SET);
        default:
            UNREACHABLE();
            return NO_INTRINSIC_ID;
    }
    return NO_INTRINSIC_ID;
}

LLVMArkInterface::IntrinsicId LLVMArkInterface::GetIntrinsicIdMath(const llvm::IntrinsicInst *inst) const
{
    auto arch = triple_.getArch();
    auto type = inst->getType();
    auto llvmId = inst->getIntrinsicID();
    auto etype = !type->isVectorTy() ? type : llvm::cast<llvm::VectorType>(type)->getElementType();
    ASSERT(etype->isFloatTy() || etype->isDoubleTy());
    switch (llvmId) {
        case llvm::Intrinsic::minimum: {
            if (arch == llvm::Triple::ArchType::aarch64) {
                return NO_INTRINSIC_ID;
            }
            auto id = etype->isFloatTy() ? RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MIN_F32
                                         : RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MIN_F64;
            return static_cast<IntrinsicId>(id);
        }
        case llvm::Intrinsic::maximum: {
            if (arch == llvm::Triple::ArchType::aarch64) {
                return NO_INTRINSIC_ID;
            }
            auto id = etype->isFloatTy() ? RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MAX_F32
                                         : RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MAX_F64;
            return static_cast<IntrinsicId>(id);
        }
        case llvm::Intrinsic::sin: {
            auto id = etype->isFloatTy() ? RuntimeInterface::IntrinsicId::INTRINSIC_MATH_SIN_F32
                                         : RuntimeInterface::IntrinsicId::INTRINSIC_MATH_SIN_F64;
            return static_cast<IntrinsicId>(id);
        }
        case llvm::Intrinsic::cos: {
            auto id = etype->isFloatTy() ? RuntimeInterface::IntrinsicId::INTRINSIC_MATH_COS_F32
                                         : RuntimeInterface::IntrinsicId::INTRINSIC_MATH_COS_F64;
            return static_cast<IntrinsicId>(id);
        }
        case llvm::Intrinsic::pow: {
            auto id = etype->isFloatTy() ? RuntimeInterface::IntrinsicId::INTRINSIC_MATH_POW_F32
                                         : RuntimeInterface::IntrinsicId::INTRINSIC_MATH_POW_F64;
            return static_cast<IntrinsicId>(id);
        }
        case llvm::Intrinsic::exp2: {
            auto id = etype->isFloatTy() ? RuntimeInterface::IntrinsicId::LIB_CALL_EXP2F
                                         : RuntimeInterface::IntrinsicId::LIB_CALL_EXP2;
            return static_cast<IntrinsicId>(id);
        }
        default:
            UNREACHABLE();
            return NO_INTRINSIC_ID;
    }
    return NO_INTRINSIC_ID;
}

llvm::StringRef LLVMArkInterface::GetRuntimeFunctionName(LLVMArkInterface::RuntimeCallType callType,
                                                         LLVMArkInterface::IntrinsicId id)
{
    if (callType == LLVMArkInterface::RuntimeCallType::INTRINSIC) {
        return llvm::StringRef(GetIntrinsicRuntimeFunctionName(id));
    }
    // sanity check
    ASSERT(callType == LLVMArkInterface::RuntimeCallType::ENTRYPOINT);
    return llvm::StringRef(GetEntrypointRuntimeFunctionName(id));
}

llvm::FunctionType *LLVMArkInterface::GetRuntimeFunctionType(llvm::StringRef name) const
{
    return runtimeFunctionTypes_.lookup(name);
}

llvm::FunctionType *LLVMArkInterface::GetOrCreateRuntimeFunctionType(llvm::LLVMContext &ctx, llvm::Module *module,
                                                                     LLVMArkInterface::RuntimeCallType callType,
                                                                     LLVMArkInterface::IntrinsicId id)
{
    auto rtFunctionName = GetRuntimeFunctionName(callType, id);
    auto rtFunctionTy = GetRuntimeFunctionType(rtFunctionName);
    if (rtFunctionTy != nullptr) {
        return rtFunctionTy;
    }

    if (callType == RuntimeCallType::INTRINSIC) {
        rtFunctionTy = GetIntrinsicDeclaration(ctx, static_cast<RuntimeInterface::IntrinsicId>(id));
    } else {
        // sanity check
        ASSERT(callType == RuntimeCallType::ENTRYPOINT);
        rtFunctionTy = GetEntrypointDeclaration(ctx, module, static_cast<RuntimeInterface::EntrypointId>(id));
    }

    ASSERT(rtFunctionTy != nullptr);
    runtimeFunctionTypes_.insert({rtFunctionName, rtFunctionTy});
    return rtFunctionTy;
}

void LLVMArkInterface::CreateRequiredIntrinsicFunctionTypes(llvm::LLVMContext &ctx)
{
    constexpr std::array REQUIRED_INTRINSIC_IDS = {
        RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MIN_F64, RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MIN_F32,
        RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MAX_F64, RuntimeInterface::IntrinsicId::INTRINSIC_MATH_MAX_F32,
        RuntimeInterface::IntrinsicId::INTRINSIC_MATH_POW_F64, RuntimeInterface::IntrinsicId::INTRINSIC_MATH_POW_F32,
        RuntimeInterface::IntrinsicId::INTRINSIC_MATH_SIN_F64, RuntimeInterface::IntrinsicId::INTRINSIC_MATH_SIN_F32,
        RuntimeInterface::IntrinsicId::INTRINSIC_MATH_COS_F64, RuntimeInterface::IntrinsicId::INTRINSIC_MATH_COS_F32,
        RuntimeInterface::IntrinsicId::LIB_CALL_FMODF,         RuntimeInterface::IntrinsicId::LIB_CALL_FMOD,
        RuntimeInterface::IntrinsicId::LIB_CALL_LDEXP,         RuntimeInterface::IntrinsicId::LIB_CALL_LDEXPF,
        RuntimeInterface::IntrinsicId::LIB_CALL_EXP2,          RuntimeInterface::IntrinsicId::LIB_CALL_EXP2F,
        RuntimeInterface::IntrinsicId::LIB_CALL_MEM_COPY,      RuntimeInterface::IntrinsicId::LIB_CALL_MEM_MOVE,
        RuntimeInterface::IntrinsicId::LIB_CALL_MEM_SET};
    for (auto id : REQUIRED_INTRINSIC_IDS) {
        auto rtFunctionName =
            GetRuntimeFunctionName(RuntimeCallType::INTRINSIC, static_cast<LLVMArkInterface::IntrinsicId>(id));
        auto rtFunctionTy = GetIntrinsicDeclaration(ctx, id);
        ASSERT(rtFunctionTy != nullptr);
        runtimeFunctionTypes_.insert({rtFunctionName, rtFunctionTy});
    }
}

void LLVMArkInterface::RememberFunctionOrigin(const llvm::Function *function, MethodPtr methodPtr)
{
    ASSERT(function != nullptr);
    ASSERT(methodPtr != nullptr);

    auto file = static_cast<panda_file::File *>(runtime_->GetBinaryFileForMethod(methodPtr));
    [[maybe_unused]] auto insertionResult = functionOrigins_.insert({function, file});
    ASSERT(insertionResult.second);
    LLVM_LOG(DEBUG, INFRA) << function->getName().data() << " defined in " << runtime_->GetFileName(methodPtr);
}

LLVMArkInterface::MethodId LLVMArkInterface::GetMethodId(const llvm::Function *caller,
                                                         const llvm::Function *callee) const
{
    ASSERT(callee != nullptr);
    ASSERT(caller != nullptr);

    auto callerOriginFile = functionOrigins_.lookup(caller);
    ASSERT_PRINT(callerOriginFile != nullptr,
                 std::string("No origin for function = '") + caller->getName().str() +
                     "'. Use RememberFunctionOrigin to store the origin before calling GetMethodId");
    auto methodIdIterator = methodIds_.find({callee, callerOriginFile});
    ASSERT_PRINT(methodIdIterator != methodIds_.end(),
                 std::string(callee->getName()) + " was not declared in callerOriginFile");

    return methodIdIterator->second;
}

void LLVMArkInterface::RememberFunctionCall(const llvm::Function *caller, const llvm::Function *callee,
                                            MethodId methodId)
{
    ASSERT(callee != nullptr);
    ASSERT(caller != nullptr);

    auto pandaFile = functionOrigins_.lookup(caller);
    ASSERT_PRINT(pandaFile != nullptr,
                 std::string("No origin for function = '") + caller->getName().str() +
                     "'. Use RememberFunctionOrigin to store the origin before calling RememberFunctionCall");
    // Assuming it is ok to declare extern function twice
    methodIds_.insert({{callee, pandaFile}, methodId});
}

bool panda::llvmbackend::LLVMArkInterface::IsRememberedCall(const llvm::Function *caller,
                                                            const llvm::Function *callee) const
{
    ASSERT(caller != nullptr);
    ASSERT(callee != nullptr);

    auto callerOriginFile = functionOrigins_.find(caller);
    ASSERT_PRINT(callerOriginFile != functionOrigins_.end(),
                 caller->getName().str() + " has no origin panda file. Use RememberFunctionOrigin for " +
                     caller->getName().str() + " before calling IsRememberedCall");

    return methodIds_.find({callee, callerOriginFile->second}) != methodIds_.end();
}

int32_t LLVMArkInterface::GetPltSlotId(const llvm::Function *caller, const llvm::Function *callee) const
{
    ASSERT(caller != nullptr);
    ASSERT(callee != nullptr);

    auto callerOriginFile = functionOrigins_.lookup(caller);
    ASSERT_PRINT(callerOriginFile != nullptr,
                 std::string("No origin for function = '") + caller->getName().str() +
                     "'. Use RememberFunctionOrigin to store the origin before calling GetPltSlotId");
    return GetAotDataFromBuilder(callerOriginFile, aotBuilder_).GetPltSlotId(GetMethodId(caller, callee));
}

int32_t LLVMArkInterface::GetClassIndexInAotGot(const llvm::Function *caller, uint32_t klassId, bool initialized)
{
    ASSERT(caller != nullptr);

    auto callerOriginFile = functionOrigins_.lookup(caller);
    ASSERT_PRINT(callerOriginFile != nullptr,
                 std::string("No origin for function = '") + caller->getName().str() +
                     "'. Use RememberFunctionOrigin to store the origin before calling GetClassIndexInAotGot");

    auto index = GetAotDataFromBuilder(callerOriginFile, aotBuilder_).GetClassSlotId(klassId);
    if (initialized) {
        return index - 1;
    }
    return index;
}

LLVMArkInterface::RuntimeCallee LLVMArkInterface::GetEntrypointCallee(EntrypointId id) const
{
    using PandaEntrypointId = panda::compiler::RuntimeInterface::EntrypointId;
    auto eid = static_cast<PandaEntrypointId>(id);
    auto functionName = GetEntrypointInternalName(eid);
    auto functionProto = GetRuntimeFunctionType(functionName);
    ASSERT(functionProto != nullptr);
    return {functionProto, functionName};
}

llvm::Function *LLVMArkInterface::GetFunctionByMethodPtr(LLVMArkInterface::MethodPtr method) const
{
    ASSERT(method != nullptr);

    return functions_.lookup(method);
}

void LLVMArkInterface::PutFunction(LLVMArkInterface::MethodPtr methodPtr, llvm::Function *function)
{
    ASSERT(function != nullptr);
    ASSERT(methodPtr != nullptr);
    // We are not expecting `tan` functions other than we are adding in LLVMIrConstructor::EmitTan()
    ASSERT(function->getName() != "tan");

    [[maybe_unused]] auto fInsertionResult = functions_.insert({methodPtr, function});
    ASSERT_PRINT(fInsertionResult.first->second == function,
                 std::string("Attempt to map '") + GetUniqMethodName(methodPtr) + "' to a function = '" +
                     function->getName().str() + "', but the method_ptr already mapped to '" +
                     fInsertionResult.first->second->getName().str() + "'");
    auto sourceLang = static_cast<uint8_t>(runtime_->GetMethodSourceLanguage(methodPtr));
    [[maybe_unused]] auto slInsertionResult = sourceLanguages_.insert({function, sourceLang});
    ASSERT(slInsertionResult.first->second == sourceLang);
}

const char *LLVMArkInterface::GetIntrinsicRuntimeFunctionName(LLVMArkInterface::IntrinsicId id) const
{
    ASSERT(id >= 0 && id < static_cast<IntrinsicId>(RuntimeInterface::IntrinsicId::COUNT));
    return GetIntrinsicInternalName(static_cast<RuntimeInterface::IntrinsicId>(id));
}

const char *LLVMArkInterface::GetEntrypointRuntimeFunctionName(LLVMArkInterface::EntrypointId id) const
{
    ASSERT(id >= 0 && id < static_cast<EntrypointId>(RuntimeInterface::EntrypointId::COUNT));
    return GetEntrypointInternalName(static_cast<RuntimeInterface::EntrypointId>(id));
}

std::string LLVMArkInterface::GetUniqMethodName(LLVMArkInterface::MethodPtr methodPtr) const
{
    ASSERT(methodPtr != nullptr);

    if (IsIrtocMode()) {
        return runtime_->GetMethodName(methodPtr);
    }
#ifndef NDEBUG
    auto uniqName = std::string(runtime_->GetMethodFullName(methodPtr, true));
    uniqName.append("_id_");
    uniqName.append(std::to_string(runtime_->GetUniqMethodId(methodPtr)));
#else
    std::stringstream ssUniqName;
    ssUniqName << "f_" << std::hex << methodPtr;
    auto uniqName = ssUniqName.str();
#endif
    return uniqName;
}

std::string LLVMArkInterface::GetUniqMethodName(const Method *method) const
{
    ASSERT(method != nullptr);

    auto casted = const_cast<Method *>(method);
    return GetUniqMethodName(static_cast<MethodPtr>(casted));
}

std::string LLVMArkInterface::GetUniqueBasicBlockName(const std::string &bbName, const std::string &uniqueSuffix)
{
    std::stringstream uniqueName;
    const std::string nameDelimiter = "..";
    auto first = bbName.find(nameDelimiter);
    if (first == std::string::npos) {
        uniqueName << bbName << "_" << uniqueSuffix;
        return uniqueName.str();
    }

    auto second = bbName.rfind(nameDelimiter);
    ASSERT(second != std::string::npos);

    uniqueName << bbName.substr(0, first + 2U) << uniqueSuffix << bbName.substr(second, bbName.size());

    return uniqueName.str();
}

uint64_t LLVMArkInterface::GetMethodStackSize(LLVMArkInterface::MethodPtr method) const
{
    std::string name = GetUniqMethodName(method);
    ASSERT(llvmStackSizes_.find(name) != llvmStackSizes_.end());
    return llvmStackSizes_.lookup(name);
}

void LLVMArkInterface::PutMethodStackSize(const llvm::Function *method, uint64_t size)
{
    std::string name = method->getName().str();
    ASSERT_PRINT(llvmStackSizes_.find(name) == llvmStackSizes_.end(), "Double inserted stack size for '" + name + "'");
    llvmStackSizes_[name] = size;
}

uint32_t LLVMArkInterface::GetVirtualRegistersCount(LLVMArkInterface::MethodPtr method) const
{
    // Registers used by user, arguments and accumulator
    return runtime_->GetMethodRegistersCount(method) + runtime_->GetMethodArgumentsCount(method) + 1;
}

uint32_t LLVMArkInterface::GetCFrameHasFloatRegsFlagMask() const
{
    return 1U << panda::CFrameLayout::HasFloatRegsFlag::START_BIT;
}

bool LLVMArkInterface::IsIrtocMode() const
{
    return aotBuilder_ == nullptr;
}

void LLVMArkInterface::AppendIrtocReturnHandler(llvm::StringRef returnHandler)
{
    irtocReturnHandlers_.push_back(returnHandler);
}

bool LLVMArkInterface::IsIrtocReturnHandler(const llvm::Function &function) const
{
    return std::find(irtocReturnHandlers_.cbegin(), irtocReturnHandlers_.cend(), function.getName()) !=
           irtocReturnHandlers_.cend();
}

}  // namespace panda::llvmbackend
