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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "assembly-parser.h"
#include "libpandabase/mem/pool_manager.h"
#include "libpandabase/utils/utf.h"
#include "libpandafile/bytecode_emitter.h"
#include "libpandafile/file.h"
#include "libpandafile/file_items.h"
#include "libpandafile/value.h"
#include "runtime/bridge/bridge.h"
#include "runtime/include/class_linker.h"
#include "runtime/include/compiler_interface.h"
#include "runtime/include/mem/allocator.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/runtime_options.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/interpreter/frame.h"
#include "runtime/mem/gc/gc.h"
#include "runtime/mem/internal_allocator.h"
#include "runtime/core/core_class_linker_extension.h"
#include "runtime/tests/class_linker_test_extension.h"
#include "runtime/tests/interpreter/test_interpreter.h"
#include "runtime/tests/interpreter/test_runtime_interface.h"
#include "runtime/include/coretypes/dyn_objects.h"
#include "runtime/include/hclass.h"
#include "runtime/handle_base-inl.h"
#include "runtime/handle_scope-inl.h"
#include "runtime/include/coretypes/native_pointer.h"
#include "runtime/tests/test_utils.h"
#include "libpandabase/test_utilities.h"

// NOLINTBEGIN(readability-magic-numbers)

namespace panda::interpreter::test {

using DynClass = panda::coretypes::DynClass;
using DynObject = panda::coretypes::DynObject;

class InterpreterTest : public testing::Test {
public:
    InterpreterTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetRunGcInPlace(true);
        options.SetVerifyCallStack(false);
        options.SetGcType("epsilon");
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~InterpreterTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    NO_COPY_SEMANTIC(InterpreterTest);
    NO_MOVE_SEMANTIC(InterpreterTest);

private:
    panda::MTManagedThread *thread_;
};

auto CreateFrame(uint32_t nregs, Method *method, Frame *prev)
{
    auto frameDeleter = [](Frame *frame) { RuntimeInterface::FreeFrame(ManagedThread::GetCurrent(), frame); };
    std::unique_ptr<Frame, decltype(frameDeleter)> frame(
        RuntimeInterface::template CreateFrame<false>(nregs, method, prev), frameDeleter);
    return frame;
}

static void InitializeFrame(Frame *f)
{
    ManagedThread::GetCurrent()->SetCurrentFrame(f);
    auto frameHandler = StaticFrameHandler(f);
    for (size_t i = 0; i < f->GetSize(); i++) {
        frameHandler.GetVReg(i).Set(static_cast<int64_t>(0));
    }
}

static Class *CreateClass(panda_file::SourceLang lang)
{
    const std::string className("Foo");
    auto runtime = Runtime::GetCurrent();
    auto etx = runtime->GetClassLinker()->GetExtension(runtime->GetLanguageContext(lang));
    auto klass = etx->CreateClass(reinterpret_cast<const uint8_t *>(className.data()), 0, 0,
                                  AlignUp(sizeof(Class), OBJECT_POINTER_SIZE));
    return klass;
}

static std::pair<PandaUniquePtr<Method>, std::unique_ptr<const panda_file::File>> CreateMethod(
    Class *klass, uint32_t accessFlags, uint32_t nargs, uint32_t nregs, uint16_t *shorty,
    const std::vector<uint8_t> &bytecode)
{
    // Create panda_file

    panda_file::ItemContainer container;
    panda_file::ClassItem *classItem = container.GetOrCreateGlobalClassItem();
    classItem->SetAccessFlags(ACC_PUBLIC);

    panda_file::StringItem *methodName = container.GetOrCreateStringItem("test");
    panda_file::PrimitiveTypeItem *retType = container.GetOrCreatePrimitiveTypeItem(panda_file::Type::TypeId::VOID);
    std::vector<panda_file::MethodParamItem> params;
    panda_file::ProtoItem *protoItem = container.GetOrCreateProtoItem(retType, params);
    panda_file::MethodItem *methodItem = classItem->AddMethod(methodName, protoItem, ACC_PUBLIC | ACC_STATIC, params);

    auto *codeItem = container.CreateItem<panda_file::CodeItem>(nregs, nargs, bytecode);
    methodItem->SetCode(codeItem);

    panda_file::MemoryWriter memWriter;
    container.Write(&memWriter);

    auto data = memWriter.GetData();

    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto buf = allocator->AllocArray<uint8_t>(data.size());
    memcpy_s(buf, data.size(), data.data(), data.size());

    os::mem::ConstBytePtr ptr(reinterpret_cast<std::byte *>(buf), data.size(), [](std::byte *buffer, size_t) noexcept {
        auto a = Runtime::GetCurrent()->GetInternalAllocator();
        a->Free(buffer);
    });
    auto pf = panda_file::File::OpenFromMemory(std::move(ptr));

    // Create method

    auto method = MakePandaUnique<Method>(klass, pf.get(), methodItem->GetFileId(), codeItem->GetFileId(),
                                          accessFlags | ACC_PUBLIC | ACC_STATIC, nargs, shorty);
    method->SetInterpreterEntryPoint();
    return {std::move(method), std::move(pf)};
}

static std::pair<PandaUniquePtr<Method>, std::unique_ptr<const panda_file::File>> CreateMethod(
    Class *klass, Frame *f, const std::vector<uint8_t> &bytecode)
{
    return CreateMethod(klass, 0, 0, f->GetSize(), nullptr, bytecode);
}

static std::unique_ptr<ClassLinker> CreateClassLinker([[maybe_unused]] ManagedThread *thread)
{
    std::vector<std::unique_ptr<ClassLinkerExtension>> extensions;
    extensions.push_back(std::make_unique<CoreClassLinkerExtension>());

    auto allocator = Runtime::GetCurrent()->GetInternalAllocator();
    auto classLinker = std::make_unique<ClassLinker>(allocator, std::move(extensions));
    if (!classLinker->Initialize()) {
        return nullptr;
    }

    return classLinker;
}

TEST_F(InterpreterTest, TestMov)
{
    BytecodeEmitter emitter;

    constexpr int64_t IMM4_MAX = 7;
    constexpr int64_t IMM8_MAX = std::numeric_limits<int8_t>::max();
    constexpr int64_t IMM16_MAX = std::numeric_limits<int16_t>::max();
    constexpr int64_t IMM32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t IMM64_MAX = std::numeric_limits<int64_t>::max();

    constexpr uint16_t V4_MAX = 15;
    constexpr uint16_t V8_MAX = std::numeric_limits<uint8_t>::max();
    constexpr uint16_t V16_MAX = std::numeric_limits<uint16_t>::max();

    ObjectHeader *obj1 = panda::mem::AllocateNullifiedPayloadString(1);
    ObjectHeader *obj2 = panda::mem::AllocateNullifiedPayloadString(100);
    ObjectHeader *obj3 = panda::mem::AllocateNullifiedPayloadString(200);

    emitter.Movi(0, IMM4_MAX);
    emitter.Movi(1, IMM8_MAX);
    emitter.Movi(2, IMM16_MAX);
    emitter.Movi(3, IMM32_MAX);
    emitter.MoviWide(4, IMM64_MAX);

    emitter.Mov(V4_MAX, V4_MAX - 1);
    emitter.Mov(V8_MAX, V8_MAX - 1);
    emitter.Mov(V16_MAX, V16_MAX - 1);

    emitter.MovWide(V4_MAX - 2, V4_MAX - 3);
    emitter.MovWide(V16_MAX - 2, V16_MAX - 3);

    emitter.MovObj(V4_MAX - 4, V4_MAX - 5);
    emitter.MovObj(V8_MAX - 4, V8_MAX - 5);
    emitter.MovObj(V16_MAX - 4, V16_MAX - 5);

    emitter.ReturnVoid();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(std::numeric_limits<uint16_t>::max() + 1, nullptr, nullptr);
    InitializeFrame(f.get());
    auto frameHandler = StaticFrameHandler(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    frameHandler.GetVReg(V4_MAX - 1).SetPrimitive(IMM64_MAX - 1);
    frameHandler.GetVReg(V8_MAX - 1).SetPrimitive(IMM64_MAX - 2);
    frameHandler.GetVReg(V16_MAX - 1).SetPrimitive(IMM64_MAX - 3);

    frameHandler.GetVReg(V4_MAX - 3).SetPrimitive(IMM64_MAX - 4);
    frameHandler.GetVReg(V16_MAX - 3).SetPrimitive(IMM64_MAX - 5);

    frameHandler.GetVReg(V4_MAX - 5).SetReference(obj1);
    frameHandler.GetVReg(V8_MAX - 5).SetReference(obj2);
    frameHandler.GetVReg(V16_MAX - 5).SetReference(obj3);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    // Check movi

    EXPECT_EQ(frameHandler.GetVReg(0).GetLong(), IMM4_MAX);
    EXPECT_FALSE(frameHandler.GetVReg(0).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(1).GetLong(), IMM8_MAX);
    EXPECT_FALSE(frameHandler.GetVReg(1).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(2).GetLong(), IMM16_MAX);
    EXPECT_FALSE(frameHandler.GetVReg(2).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(3).GetLong(), IMM32_MAX);
    EXPECT_FALSE(frameHandler.GetVReg(3).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(4).GetLong(), IMM64_MAX);
    EXPECT_FALSE(frameHandler.GetVReg(4).HasObject());

    // Check mov

    EXPECT_EQ(frameHandler.GetVReg(V4_MAX).Get(), static_cast<int32_t>(IMM64_MAX - 1));
    EXPECT_FALSE(frameHandler.GetVReg(V4_MAX).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(V8_MAX).Get(), static_cast<int32_t>(IMM64_MAX - 2));
    EXPECT_FALSE(frameHandler.GetVReg(V8_MAX).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(V16_MAX).Get(), static_cast<int32_t>(IMM64_MAX - 3));
    EXPECT_FALSE(frameHandler.GetVReg(V16_MAX).HasObject());

    // Check mov.64

    EXPECT_EQ(frameHandler.GetVReg(V4_MAX - 2).GetLong(), IMM64_MAX - 4);
    EXPECT_FALSE(frameHandler.GetVReg(V4_MAX - 2).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(V16_MAX - 2).GetLong(), IMM64_MAX - 5);
    EXPECT_FALSE(frameHandler.GetVReg(V16_MAX - 2).HasObject());

    // Check mov.obj

    EXPECT_EQ(frameHandler.GetVReg(V4_MAX - 4).GetReference(), obj1);
    EXPECT_TRUE(frameHandler.GetVReg(V4_MAX - 4).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(V8_MAX - 4).GetReference(), obj2);
    EXPECT_TRUE(frameHandler.GetVReg(V8_MAX - 4).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(V16_MAX - 4).GetReference(), obj3);
    EXPECT_TRUE(frameHandler.GetVReg(V16_MAX - 4).HasObject());
}

TEST_F(InterpreterTest, TestLoadStoreAccumulator)
{
    BytecodeEmitter emitter;

    constexpr int64_t IMM8_MAX = std::numeric_limits<int8_t>::max();
    constexpr int64_t IMM16_MAX = std::numeric_limits<int16_t>::max();
    constexpr int64_t IMM32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t IMM64_MAX = std::numeric_limits<int64_t>::max();

    ObjectHeader *obj = panda::mem::AllocateNullifiedPayloadString(10);

    emitter.Ldai(IMM8_MAX);
    emitter.Sta(0);

    emitter.Ldai(IMM16_MAX);
    emitter.Sta(1);

    emitter.Ldai(IMM32_MAX);
    emitter.Sta(2);

    emitter.LdaiWide(IMM64_MAX);
    emitter.StaWide(3);

    emitter.Lda(4);
    emitter.Sta(5);

    emitter.LdaWide(6);
    emitter.StaWide(7);

    emitter.LdaObj(8);
    emitter.StaObj(9);

    emitter.ReturnVoid();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());
    auto frameHandler = StaticFrameHandler(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    frameHandler.GetVReg(4).SetPrimitive(IMM64_MAX - 1);
    frameHandler.GetVReg(6).SetPrimitive(IMM64_MAX - 2);
    frameHandler.GetVReg(8).SetReference(obj);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(frameHandler.GetVReg(0).Get(), static_cast<int32_t>(IMM8_MAX));
    EXPECT_FALSE(frameHandler.GetVReg(0).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(1).Get(), static_cast<int32_t>(IMM16_MAX));
    EXPECT_FALSE(frameHandler.GetVReg(1).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(2).Get(), static_cast<int32_t>(IMM32_MAX));
    EXPECT_FALSE(frameHandler.GetVReg(2).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(3).GetLong(), IMM64_MAX);
    EXPECT_FALSE(frameHandler.GetVReg(3).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(5).Get(), static_cast<int32_t>(IMM64_MAX - 1));
    EXPECT_FALSE(frameHandler.GetVReg(5).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(7).GetLong(), IMM64_MAX - 2);
    EXPECT_FALSE(frameHandler.GetVReg(7).HasObject());

    EXPECT_EQ(frameHandler.GetVReg(9).GetReference(), obj);
    EXPECT_TRUE(frameHandler.GetVReg(9).HasObject());
}

TEST_F(InterpreterTest, TestLoadString)
{
    pandasm::Parser p;
    std::string source = R"(
    .record panda.String <external>
    .function panda.String foo(){
        lda.str "TestLoadString"
        return.obj
    }
    )";

    auto res = p.Parse(source);
    auto classPf = pandasm::AsmEmitter::Emit(res.Value());

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(classLinker, nullptr);

    classLinker->AddPandaFile(std::move(classPf));

    PandaString descriptor;
    auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);

    Class *cls = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
    Method *method = cls->GetClassMethod(utf::CStringAsMutf8("foo"));
    const uint8_t *methodData = method->GetInstructions();

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());
    f->SetMethod(method);

    Execute(ManagedThread::GetCurrent(), methodData, f.get());
    EXPECT_TRUE(f->GetAccAsVReg().HasObject());

    PandaString strSample = "TestLoadString";
    panda::coretypes::String *strCore = panda::coretypes::String::Cast(f->GetAccAsVReg().GetReference());

    const char *str = reinterpret_cast<const char *>(strCore->GetDataMUtf8());
    size_t strLen = strCore->GetMUtf8Length() - 1;  // Reserved zero.
    PandaString strTst(str, strLen);

    EXPECT_EQ(strSample, strTst);
}

void TestUnimpelemented(const std::function<void(BytecodeEmitter *)> &emit)
{
    BytecodeEmitter emitter;

    emit(&emitter);

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    EXPECT_DEATH_IF_SUPPORTED(Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get()), "");
}

TEST_F(InterpreterTest, LoadType)
{
    BytecodeEmitter emitter;

    pandasm::Parser p;
    auto source = R"(
        .record R {}
    )";

    auto res = p.Parse(source);
    auto classPf = pandasm::AsmEmitter::Emit(res.Value());

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(classLinker, nullptr);

    classLinker->AddPandaFile(std::move(classPf));

    PandaString descriptor;
    auto *thread = ManagedThread::GetCurrent();
    auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(classLinker->InitializeClass(thread, objectClass));

    emitter.LdaType(RuntimeInterface::TYPE_ID.AsIndex());
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    RuntimeInterface::SetupResolvedClass(objectClass);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedClass(nullptr);

    EXPECT_EQ(coretypes::Class::FromRuntimeClass(objectClass), f->GetAccAsVReg().GetReference());
}

void TestFcmp(double v1, double v2, int64_t value, bool isCmpg = false)
{
    std::ostringstream ss;
    if (isCmpg) {
        ss << "Test fcmpg.64";
    } else {
        ss << "Test fcmpl.64";
    }
    ss << ", v1 = " << v1 << ", v2 = " << v2;

    BytecodeEmitter emitter;

    if (isCmpg) {
        emitter.FcmpgWide(0);
    } else {
        emitter.FcmplWide(0);
    }
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    f->GetAccAsVReg().SetPrimitive(v1);
    auto frameHandler = StaticFrameHandler(f.get());
    frameHandler.GetVReg(0).SetPrimitive(v2);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetAccAsVReg().GetLong(), value) << ss.str();
    EXPECT_FALSE(f->GetAccAsVReg().HasObject()) << ss.str();
}

TEST_F(InterpreterTest, TestFcmp)
{
    TestFcmp(nan(""), 1.0, 1, true);
    TestFcmp(1.0, nan(""), 1, true);
    TestFcmp(nan(""), nan(""), 1, true);
    TestFcmp(1.0, 2.0, -1, true);
    TestFcmp(1.0, 1.0, 0, true);
    TestFcmp(3.0, 2.0, 1, true);

    TestFcmp(nan(""), 1.0, -1);
    TestFcmp(1.0, nan(""), -1);
    TestFcmp(nan(""), nan(""), -1);
    TestFcmp(1.0, 2.0, -1);
    TestFcmp(1.0, 1.0, 0);
    TestFcmp(3.0, 2.0, 1);
}

void TestConditionalJmp(const std::string &mnemonic, int64_t v1, int64_t v2, int64_t r,
                        const std::function<void(BytecodeEmitter *, uint8_t, const Label &)> &emit)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v1 = " << v1 << ", v2 = " << v2;

    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();

        emit(&emitter, 0, label);
        emitter.MoviWide(1, -1);
        emitter.ReturnVoid();
        emitter.Bind(label);
        emitter.MoviWide(1, 1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        f->GetAccAsVReg().SetPrimitive(v1);
        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).SetPrimitive(v2);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(frameHandler.GetVReg(1).GetLong(), r) << ss.str();
    }

    {
        BytecodeEmitter emitter;
        Label label1 = emitter.CreateLabel();
        Label label2 = emitter.CreateLabel();

        emitter.Jmp(label1);
        emitter.Bind(label2);
        emitter.MoviWide(1, 1);
        emitter.ReturnVoid();
        emitter.Bind(label1);
        emit(&emitter, 0, label2);
        emitter.MoviWide(1, -1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        f->GetAccAsVReg().SetPrimitive(v1);
        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).SetPrimitive(v2);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(frameHandler.GetVReg(1).GetLong(), r) << ss.str();
        EXPECT_EQ(method->GetHotnessCounter(),
                  (r == 1 ? (Runtime::GetCurrent()->IsJitEnabled() ? 1499U : std::numeric_limits<int16_t>::max() - 1)
                          : (Runtime::GetCurrent()->IsJitEnabled() ? 1500U : std::numeric_limits<int16_t>::max())))
            << ss.str();
    }
}

void TestConditionalJmpz(const std::string &mnemonic, int64_t v, int64_t r,
                         const std::function<void(BytecodeEmitter *, const Label &)> &emit)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v = " << v;

    {
        BytecodeEmitter emitter;
        Label label = emitter.CreateLabel();

        emit(&emitter, label);
        emitter.MoviWide(0, -1);
        emitter.ReturnVoid();
        emitter.Bind(label);
        emitter.MoviWide(0, 1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        f->GetAccAsVReg().SetPrimitive(v);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        auto frameHandler = StaticFrameHandler(f.get());
        EXPECT_EQ(frameHandler.GetVReg(0).GetLong(), r) << ss.str();
    }

    {
        BytecodeEmitter emitter;
        Label label1 = emitter.CreateLabel();
        Label label2 = emitter.CreateLabel();

        emitter.Jmp(label1);
        emitter.Bind(label2);
        emitter.MoviWide(0, 1);
        emitter.ReturnVoid();
        emitter.Bind(label1);
        emit(&emitter, label2);
        emitter.MoviWide(0, -1);
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        f->GetAccAsVReg().SetPrimitive(v);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        auto frameHandler = StaticFrameHandler(f.get());
        EXPECT_EQ(frameHandler.GetVReg(0).GetLong(), r) << ss.str();
        EXPECT_EQ(method->GetHotnessCounter(),
                  (r == 1 ? (Runtime::GetCurrent()->IsJitEnabled() ? 1499U : std::numeric_limits<int16_t>::max() - 1)
                          : (Runtime::GetCurrent()->IsJitEnabled() ? 1500U : std::numeric_limits<int16_t>::max())))
            << ss.str();
    }
}

TEST_F(InterpreterTest, TestConditionalJumps)
{
    // Test jmpz

    TestConditionalJmpz("jeqz", 0, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jeqz(label); });
    TestConditionalJmpz("jeqz", 1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jeqz(label); });
    TestConditionalJmpz("jeqz", -1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jeqz(label); });

    TestConditionalJmpz("jnez", 0, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jnez(label); });
    TestConditionalJmpz("jnez", 1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jnez(label); });
    TestConditionalJmpz("jnez", -1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jnez(label); });

    TestConditionalJmpz("jltz", -1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jltz(label); });
    TestConditionalJmpz("jltz", 0, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jltz(label); });
    TestConditionalJmpz("jltz", 1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jltz(label); });

    TestConditionalJmpz("jgtz", 1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgtz(label); });
    TestConditionalJmpz("jgtz", 0, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgtz(label); });
    TestConditionalJmpz("jgtz", -1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgtz(label); });

    TestConditionalJmpz("jlez", -1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jlez(label); });
    TestConditionalJmpz("jlez", 0, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jlez(label); });
    TestConditionalJmpz("jlez", 1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jlez(label); });

    TestConditionalJmpz("jgez", 1, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgez(label); });
    TestConditionalJmpz("jgez", 0, 1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgez(label); });
    TestConditionalJmpz("jgez", -1, -1, [](BytecodeEmitter *emitter, const Label &label) { emitter->Jgez(label); });

    // Test jmp

    TestConditionalJmp("jeq", 2, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jeq(reg, label); });
    TestConditionalJmp("jeq", 1, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jeq(reg, label); });
    TestConditionalJmp("jeq", 2, 1, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jeq(reg, label); });

    TestConditionalJmp("jne", 2, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jne(reg, label); });
    TestConditionalJmp("jne", 1, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jne(reg, label); });
    TestConditionalJmp("jne", 2, 1, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jne(reg, label); });

    TestConditionalJmp("jlt", 2, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jlt(reg, label); });
    TestConditionalJmp("jlt", 1, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jlt(reg, label); });
    TestConditionalJmp("jlt", 2, 1, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jlt(reg, label); });

    TestConditionalJmp("jgt", 2, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jgt(reg, label); });
    TestConditionalJmp("jgt", 1, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jgt(reg, label); });
    TestConditionalJmp("jgt", 2, 1, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jgt(reg, label); });

    TestConditionalJmp("jle", 2, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jle(reg, label); });
    TestConditionalJmp("jle", 1, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jle(reg, label); });
    TestConditionalJmp("jle", 2, 1, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jle(reg, label); });

    TestConditionalJmp("jge", 2, 2, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jge(reg, label); });
    TestConditionalJmp("jge", 1, 2, -1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jge(reg, label); });
    TestConditionalJmp("jge", 2, 1, 1,
                       [](BytecodeEmitter *emitter, uint8_t reg, const Label &label) { emitter->Jge(reg, label); });
}

template <class T, class R>
void TestUnaryOp(const std::string &mnemonic, T v, R r, const std::function<void(BytecodeEmitter *)> &emit)
{
    std::ostringstream ss;
    ss << "Test " << mnemonic << " with v = " << v;

    BytecodeEmitter emitter;

    emit(&emitter);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    f->GetAccAsVReg().SetPrimitive(v);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(f->GetAccAsVReg().GetAs<R>(), r) << ss.str();
}

TEST_F(InterpreterTest, TestUnaryOp)
{
    constexpr int64_t I32_MIN = std::numeric_limits<int32_t>::min();
    constexpr int64_t I64_MIN = std::numeric_limits<int64_t>::min();

    TestUnaryOp<int64_t, int64_t>("neg", I64_MIN + 1, -(I64_MIN + 1),
                                  [](BytecodeEmitter *emitter) { emitter->NegWide(); });

    TestUnaryOp<int32_t, int64_t>("neg", I32_MIN + 1, -(I32_MIN + 1), [](BytecodeEmitter *emitter) { emitter->Neg(); });

    TestUnaryOp<double, double>("fneg", 1.0, -1.0, [](BytecodeEmitter *emitter) { emitter->FnegWide(); });

    TestUnaryOp<int64_t, int64_t>("not", 0, 0xffffffffffffffff, [](BytecodeEmitter *emitter) { emitter->NotWide(); });

    TestUnaryOp<int32_t, int32_t>("not", 0, 0xffffffff, [](BytecodeEmitter *emitter) { emitter->Not(); });
}

TEST_F(InterpreterTest, TestInci)
{
    BytecodeEmitter emitter;
    emitter.Inci(0, 2);
    emitter.Inci(1, -3);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    auto frameHandler = StaticFrameHandler(f.get());
    frameHandler.GetVReg(0).SetPrimitive(-2);
    frameHandler.GetVReg(1).SetPrimitive(3);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    EXPECT_EQ(frameHandler.GetVReg(0).GetAs<int32_t>(), 0);
    EXPECT_EQ(frameHandler.GetVReg(1).GetAs<int32_t>(), 0);
}

TEST_F(InterpreterTest, TestCast)
{
    constexpr int64_t I64_MAX = std::numeric_limits<int64_t>::max();
    constexpr int32_t I32_MAX = std::numeric_limits<int32_t>::max();
    constexpr int64_t I64_MIN = std::numeric_limits<int64_t>::min();
    constexpr int32_t I32_MIN = std::numeric_limits<int32_t>::min();

    constexpr double F64_MAX = std::numeric_limits<double>::max();
    constexpr double F64_PINF = std::numeric_limits<double>::infinity();
    constexpr double F64_NINF = -F64_PINF;

    double f64 = 64.0;

    TestUnaryOp("i32toi64", I32_MAX, static_cast<int64_t>(I32_MAX),
                [](BytecodeEmitter *emitter) { emitter->I32toi64(); });

    TestUnaryOp("i32tof64", I32_MAX, static_cast<double>(I32_MAX),
                [](BytecodeEmitter *emitter) { emitter->I32tof64(); });

    TestUnaryOp("i64toi32", I64_MAX, static_cast<int32_t>(I64_MAX),
                [](BytecodeEmitter *emitter) { emitter->I64toi32(); });

    TestUnaryOp("i64tof64", I64_MAX, static_cast<double>(I64_MAX),
                [](BytecodeEmitter *emitter) { emitter->I64tof64(); });

    TestUnaryOp("F64toi32", F64_MAX, I32_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", F64_PINF, I32_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", -F64_MAX, I32_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", F64_NINF, I32_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", nan(""), 0, [](BytecodeEmitter *emitter) { emitter->F64toi32(); });
    TestUnaryOp("F64toi32", f64, static_cast<int32_t>(f64), [](BytecodeEmitter *emitter) { emitter->F64toi32(); });

    TestUnaryOp("F64toi64", F64_MAX, I64_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", F64_PINF, I64_MAX, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", -F64_MAX, I64_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", F64_NINF, I64_MIN, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", nan(""), 0, [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
    TestUnaryOp("F64toi64", f64, static_cast<int64_t>(f64), [](BytecodeEmitter *emitter) { emitter->F64toi64(); });
}

// clang-format off

template <panda_file::Type::TypeId TYPE_ID>
struct ArrayComponentTypeHelper {
    using Type = std::conditional_t<TYPE_ID == panda_file::Type::TypeId::U1, uint8_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::I8, int8_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::U8, uint8_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::I16, int16_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::U16, uint16_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::I32, int32_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::U32, uint32_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::I64, int64_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::U64, uint64_t,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::F32, float,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::F64, double,
                 std::conditional_t<TYPE_ID == panda_file::Type::TypeId::REFERENCE, ObjectHeader*, void>>>>>>>>>>>>;
};

// clang-format on

template <panda_file::Type::TypeId TYPE_ID>
using ArrayComponentTypeHelperT = typename ArrayComponentTypeHelper<TYPE_ID>::Type;

template <panda_file::Type::TypeId TYPE_ID>
struct ArrayStoredTypeHelperT {
    using Type = typename ArrayComponentTypeHelper<TYPE_ID>::Type;
};

template <>
struct ArrayStoredTypeHelperT<panda_file::Type::TypeId::REFERENCE> {
    using Type = ObjectPointerType;
};

template <panda_file::Type::TypeId TYPE_ID>
typename ArrayStoredTypeHelperT<TYPE_ID>::Type CastIfRef(ArrayComponentTypeHelperT<TYPE_ID> value)
{
    if constexpr (TYPE_ID == panda_file::Type::TypeId::REFERENCE) {
        return static_cast<ObjectPointerType>(reinterpret_cast<uintptr_t>(value));
    } else {
        return value;
    }
}

coretypes::Array *AllocArray(Class *cls, [[maybe_unused]] size_t elemSize, size_t length)
{
    return coretypes::Array::Create(cls, length);
}

ObjectHeader *AllocObject(Class *cls)
{
    return ObjectHeader::Create(cls);
}

template <class T>
static T GetStoreValue([[maybe_unused]] Class *cls)
{
    if constexpr (std::is_same_v<T, ObjectHeader *>) {
        return AllocObject(cls);
    }

    return std::numeric_limits<T>::max();
}

template <class T>
static T GetLoadValue([[maybe_unused]] Class *cls)
{
    if constexpr (std::is_same_v<T, ObjectHeader *>) {
        return AllocObject(cls);
    }

    return std::numeric_limits<T>::min() + 1;
}

PandaString GetArrayClassName(panda_file::Type::TypeId componentTypeId)
{
    PandaString descriptor;

    if (componentTypeId == panda_file::Type::TypeId::REFERENCE) {
        ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8("panda.Object"), 1, &descriptor);
        return descriptor;
    }

    ClassHelper::GetPrimitiveArrayDescriptor(panda_file::Type(componentTypeId), 1, &descriptor);
    return descriptor;
}

template <panda_file::Type::TypeId COMPONENT_TYPE_ID>
static void TestArray()
{
    std::ostringstream ss;
    ss << "Test with component type id " << static_cast<uint32_t>(COMPONENT_TYPE_ID);

    using ComponentType = ArrayComponentTypeHelperT<COMPONENT_TYPE_ID>;
    using StoredType = typename ArrayStoredTypeHelperT<COMPONENT_TYPE_ID>::Type;

    BytecodeEmitter emitter;

    constexpr int64_t ARRAY_LENGTH = 10;
    constexpr size_t STORE_IDX = ARRAY_LENGTH - 1;
    constexpr size_t LOAD_IDX = 0;

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(classLinker, nullptr) << ss.str();

    auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    PandaString arrayClassName = GetArrayClassName(COMPONENT_TYPE_ID);
    Class *arrayClass = classLinker->GetExtension(ctx)->GetClass(utf::CStringAsMutf8(arrayClassName.c_str()));
    Class *elemClass = arrayClass->GetComponentType();

    const auto storeValue = GetStoreValue<ComponentType>(elemClass);
    const auto loadValue = GetLoadValue<ComponentType>(elemClass);

    emitter.Movi(0, ARRAY_LENGTH);
    emitter.Newarr(1, 0, RuntimeInterface::TYPE_ID.AsIndex());

    if constexpr (COMPONENT_TYPE_ID == panda_file::Type::TypeId::REFERENCE) {
        emitter.LdaObj(4);
    } else if constexpr (COMPONENT_TYPE_ID == panda_file::Type::TypeId::F32) {
        emitter.Fldai(bit_cast<int32_t>(storeValue));
    } else if constexpr (COMPONENT_TYPE_ID == panda_file::Type::TypeId::F64) {
        emitter.FldaiWide(bit_cast<int64_t>(storeValue));
    } else {
        emitter.LdaiWide(static_cast<int64_t>(storeValue));
    }

    emitter.Movi(2, STORE_IDX);

    if constexpr (COMPONENT_TYPE_ID != panda_file::Type::TypeId::REFERENCE) {
        switch (COMPONENT_TYPE_ID) {
            case panda_file::Type::TypeId::U1:
            case panda_file::Type::TypeId::U8: {
                emitter.Starr8(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarru8(1);
                break;
            }
            case panda_file::Type::TypeId::I8: {
                emitter.Starr8(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarr8(1);
                break;
            }
            case panda_file::Type::TypeId::U16: {
                emitter.Starr16(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarru16(1);
                break;
            }
            case panda_file::Type::TypeId::I16: {
                emitter.Starr16(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarr16(1);
                break;
            }
            case panda_file::Type::TypeId::U32:
            case panda_file::Type::TypeId::I32: {
                emitter.Starr(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.Ldarr(1);
                break;
            }
            case panda_file::Type::TypeId::U64:
            case panda_file::Type::TypeId::I64: {
                emitter.StarrWide(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.LdarrWide(1);
                break;
            }
            case panda_file::Type::TypeId::F32: {
                emitter.Fstarr32(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.Fldarr32(1);
                break;
            }
            case panda_file::Type::TypeId::F64: {
                emitter.FstarrWide(1, 2);
                emitter.Ldai(LOAD_IDX);
                emitter.FldarrWide(1);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }
    } else {
        emitter.StarrObj(1, 2);
        emitter.Ldai(LOAD_IDX);
        emitter.LdarrObj(1);
    }

    if constexpr (COMPONENT_TYPE_ID != panda_file::Type::TypeId::REFERENCE) {
        emitter.StaWide(3);
    } else {
        emitter.StaObj(3);
    }

    emitter.Lenarr(1);
    emitter.Return();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    auto frameHandler = StaticFrameHandler(f.get());
    if constexpr (COMPONENT_TYPE_ID == panda_file::Type::TypeId::REFERENCE) {
        frameHandler.GetVReg(4).SetReference(storeValue);
    }

    coretypes::Array *array = AllocArray(arrayClass, sizeof(StoredType), ARRAY_LENGTH);
    array->Set<ComponentType>(LOAD_IDX, loadValue);

    RuntimeInterface::SetupResolvedClass(arrayClass);
    RuntimeInterface::SetupArrayClass(arrayClass);
    RuntimeInterface::SetupArrayLength(ARRAY_LENGTH);
    RuntimeInterface::SetupArrayObject(array);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedClass(nullptr);
    RuntimeInterface::SetupArrayClass(nullptr);
    RuntimeInterface::SetupArrayObject(nullptr);

    ASSERT_EQ(f->GetAccAsVReg().Get(), ARRAY_LENGTH) << ss.str();

    auto *result = static_cast<coretypes::Array *>(frameHandler.GetVReg(1).GetReference());
    EXPECT_EQ(result, array) << ss.str();

    EXPECT_EQ(frameHandler.GetVReg(3).GetAs<ComponentType>(), loadValue) << ss.str();

    std::vector<StoredType> data(ARRAY_LENGTH);
    data[LOAD_IDX] = CastIfRef<COMPONENT_TYPE_ID>(loadValue);
    data[STORE_IDX] = CastIfRef<COMPONENT_TYPE_ID>(storeValue);

    EXPECT_THAT(data, ::testing::ElementsAreArray(reinterpret_cast<StoredType *>(array->GetData()), ARRAY_LENGTH))
        << ss.str();
}

TEST_F(InterpreterTest, TestArray)
{
    TestArray<panda_file::Type::TypeId::U1>();
    TestArray<panda_file::Type::TypeId::I8>();
    TestArray<panda_file::Type::TypeId::U8>();
    TestArray<panda_file::Type::TypeId::I16>();
    TestArray<panda_file::Type::TypeId::U16>();
    TestArray<panda_file::Type::TypeId::I32>();
    TestArray<panda_file::Type::TypeId::U32>();
    TestArray<panda_file::Type::TypeId::I64>();
    TestArray<panda_file::Type::TypeId::U64>();
    TestArray<panda_file::Type::TypeId::F32>();
    TestArray<panda_file::Type::TypeId::F64>();
    TestArray<panda_file::Type::TypeId::REFERENCE>();
}

ObjectHeader *AllocObject(BaseClass *cls)
{
    return ObjectHeader::Create(cls);
}

TEST_F(InterpreterTest, TestNewobj)
{
    BytecodeEmitter emitter;

    emitter.Newobj(0, RuntimeInterface::TYPE_ID.AsIndex());
    emitter.LdaObj(0);
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    pandasm::Parser p;
    auto source = R"(
        .record R {}
    )";

    auto res = p.Parse(source);
    auto classPf = pandasm::AsmEmitter::Emit(res.Value());

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(classLinker, nullptr);

    classLinker->AddPandaFile(std::move(classPf));

    PandaString descriptor;
    auto *thread = ManagedThread::GetCurrent();
    auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(classLinker->InitializeClass(thread, objectClass));

    ObjectHeader *obj = AllocObject(objectClass);

    RuntimeInterface::SetupResolvedClass(objectClass);
    RuntimeInterface::SetupObjectClass(objectClass);
    RuntimeInterface::SetupObject(obj);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedClass(nullptr);
    RuntimeInterface::SetupObjectClass(nullptr);
    RuntimeInterface::SetupObject(nullptr);

    EXPECT_EQ(obj, f->GetAccAsVReg().GetReference());
}

TEST_F(InterpreterTest, TestInitobj)
{
    {
        BytecodeEmitter emitter;

        emitter.InitobjShort(0, 2, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}

            .function void R.ctor(R a0, i32 a1, i32 a2) <static> {
                return.void
            }
        )";

        auto res = p.Parse(source);
        auto classPf = pandasm::AsmEmitter::Emit(res.Value());

        auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(classLinker, nullptr);

        classLinker->AddPandaFile(std::move(classPf));

        PandaString descriptor;

        auto *thread = ManagedThread::GetCurrent();
        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(classLinker->InitializeClass(thread, objectClass));

        Method *ctor = objectClass->GetMethods().data();
        ObjectHeader *obj = AllocObject(objectClass);

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).Set(10);
        frameHandler.GetVReg(2).Set(20);

        bool hasErrors = false;

        RuntimeInterface::SetupInvokeMethodHandler(
            [&]([[maybe_unused]] ManagedThread *t, Method *m, Value *args) -> Value {
                if (m != ctor) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                Span<Value> sp(args, m->GetNumArgs());
                if (sp[0].GetAs<ObjectHeader *>() != obj) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[1].GetAs<int32_t>() != frameHandler.GetVReg(0).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[2].GetAs<int32_t>() != frameHandler.GetVReg(2).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                return Value(nullptr);
            });

        RuntimeInterface::SetupResolvedMethod(ctor);
        RuntimeInterface::SetupResolvedClass(objectClass);
        RuntimeInterface::SetupObjectClass(objectClass);
        RuntimeInterface::SetupObject(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        ASSERT_FALSE(hasErrors);

        RuntimeInterface::SetupInvokeMethodHandler({});
        RuntimeInterface::SetupResolvedMethod(nullptr);
        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupObjectClass(nullptr);
        RuntimeInterface::SetupObject(nullptr);

        EXPECT_EQ(obj, f->GetAccAsVReg().GetReference());
    }

    {
        BytecodeEmitter emitter;

        emitter.Initobj(0, 2, 3, 5, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}

            .function void R.ctor(R a0, i32 a1, i32 a2, i32 a3, i32 a4) <static> {
                return.void
            }
        )";

        auto res = p.Parse(source);
        auto classPf = pandasm::AsmEmitter::Emit(res.Value());

        auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(classLinker, nullptr);

        classLinker->AddPandaFile(std::move(classPf));

        PandaString descriptor;

        auto *thread = ManagedThread::GetCurrent();
        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(classLinker->InitializeClass(thread, objectClass));

        Method *ctor = objectClass->GetMethods().data();
        ObjectHeader *obj = AllocObject(objectClass);

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).Set(10);
        frameHandler.GetVReg(2).Set(20);
        frameHandler.GetVReg(3).Set(30);
        frameHandler.GetVReg(5).Set(40);

        bool hasErrors = false;

        RuntimeInterface::SetupInvokeMethodHandler(
            [&]([[maybe_unused]] ManagedThread *t, Method *m, Value *args) -> Value {
                if (m != ctor) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                Span<Value> sp(args, m->GetNumArgs());
                if (sp[0].GetAs<ObjectHeader *>() != obj) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[1].GetAs<int32_t>() != frameHandler.GetVReg(0).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[2].GetAs<int32_t>() != frameHandler.GetVReg(2).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[3].GetAs<int32_t>() != frameHandler.GetVReg(3).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[4].GetAs<int32_t>() != frameHandler.GetVReg(5).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                return Value(nullptr);
            });

        RuntimeInterface::SetupResolvedMethod(ctor);
        RuntimeInterface::SetupResolvedClass(objectClass);
        RuntimeInterface::SetupObjectClass(objectClass);
        RuntimeInterface::SetupObject(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        ASSERT_FALSE(hasErrors);

        RuntimeInterface::SetupInvokeMethodHandler({});
        RuntimeInterface::SetupResolvedMethod(nullptr);
        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupObjectClass(nullptr);
        RuntimeInterface::SetupObject(nullptr);

        EXPECT_EQ(obj, f->GetAccAsVReg().GetReference());
    }

    {
        BytecodeEmitter emitter;

        emitter.InitobjRange(2, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record R {}

            .function void R.ctor(R a0, i32 a1, i32 a2, i32 a3, i32 a4, i32 a5) <static> {
                return.void
            }
        )";

        auto res = p.Parse(source);
        auto classPf = pandasm::AsmEmitter::Emit(res.Value());

        auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(classLinker, nullptr);

        classLinker->AddPandaFile(std::move(classPf));

        PandaString descriptor;

        auto *thread = ManagedThread::GetCurrent();
        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
        ASSERT_TRUE(classLinker->InitializeClass(thread, objectClass));

        Method *ctor = objectClass->GetMethods().data();
        ObjectHeader *obj = AllocObject(objectClass);

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(2).Set(10);
        frameHandler.GetVReg(3).Set(20);
        frameHandler.GetVReg(4).Set(30);
        frameHandler.GetVReg(5).Set(40);
        frameHandler.GetVReg(6).Set(50);

        bool hasErrors = false;

        RuntimeInterface::SetupInvokeMethodHandler(
            [&]([[maybe_unused]] ManagedThread *t, Method *m, Value *args) -> Value {
                if (m != ctor) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                Span<Value> sp(args, m->GetNumArgs());
                if (sp[0].GetAs<ObjectHeader *>() != obj) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[1].GetAs<int32_t>() != frameHandler.GetVReg(2).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[2].GetAs<int32_t>() != frameHandler.GetVReg(3).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[3].GetAs<int32_t>() != frameHandler.GetVReg(4).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[4].GetAs<int32_t>() != frameHandler.GetVReg(5).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                if (sp[5].GetAs<int32_t>() != frameHandler.GetVReg(6).Get()) {
                    hasErrors = true;
                    return Value(nullptr);
                }

                return Value(nullptr);
            });

        RuntimeInterface::SetupResolvedMethod(ctor);
        RuntimeInterface::SetupResolvedClass(objectClass);
        RuntimeInterface::SetupObjectClass(objectClass);
        RuntimeInterface::SetupObject(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        ASSERT_FALSE(hasErrors);

        RuntimeInterface::SetupInvokeMethodHandler({});
        RuntimeInterface::SetupResolvedMethod(nullptr);
        RuntimeInterface::SetupResolvedClass(nullptr);
        RuntimeInterface::SetupObjectClass(nullptr);
        RuntimeInterface::SetupObject(nullptr);

        EXPECT_EQ(obj, f->GetAccAsVReg().GetReference());
    }
}

void TestLoadStoreField(bool isStatic)
{
    BytecodeEmitter emitter;

    if (isStatic) {
        emitter.Ldstatic(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaWide(1);
        emitter.LdaWide(2);
        emitter.Ststatic(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.Ldstatic(RuntimeInterface::FIELD_ID.AsIndex());
    } else {
        emitter.Ldobj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaWide(1);
        emitter.LdaWide(2);
        emitter.Stobj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.Ldobj(0, RuntimeInterface::FIELD_ID.AsIndex());
    }
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    pandasm::Parser p;
    std::string source;

    if (isStatic) {
        source = R"(
            .record R {
                u1  sf_u1  <static>
                i8  sf_i8  <static>
                u8  sf_u8  <static>
                i16 sf_i16 <static>
                u16 sf_u16 <static>
                i32 sf_i32 <static>
                u32 sf_u32 <static>
                i64 sf_i64 <static>
                u64 sf_u64 <static>
                f32 sf_f32 <static>
                f64 sf_f64 <static>
            }
        )";
    } else {
        source = R"(
            .record R {
                u1  if_u1
                i8  if_i8
                u8  if_u8
                i16 if_i16
                u16 if_u16
                i32 if_i32
                u32 if_u32
                i64 if_i64
                u64 if_u64
                f32 if_f32
                f64 if_f64
            }
        )";
    }

    auto res = p.Parse(source);
    auto classPf = pandasm::AsmEmitter::Emit(res.Value());

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(classLinker, nullptr);

    classLinker->AddPandaFile(std::move(classPf));

    PandaString descriptor;

    auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(classLinker->InitializeClass(ManagedThread::GetCurrent(), objectClass));
    ObjectHeader *obj = nullptr;

    auto frameHandler = StaticFrameHandler(f.get());
    if (!isStatic) {
        obj = AllocObject(objectClass);
        frameHandler.GetVReg(0).SetReference(obj);
    }

    std::vector<panda_file::Type::TypeId> types {
        panda_file::Type::TypeId::U1,  panda_file::Type::TypeId::I8,  panda_file::Type::TypeId::U8,
        panda_file::Type::TypeId::I16, panda_file::Type::TypeId::U16, panda_file::Type::TypeId::I32,
        panda_file::Type::TypeId::U32, panda_file::Type::TypeId::I64, panda_file::Type::TypeId::U64,
        panda_file::Type::TypeId::F32, panda_file::Type::TypeId::F64};

    Span<Field> fields = isStatic ? objectClass->GetStaticFields() : objectClass->GetInstanceFields();
    for (Field &field : fields) {
        std::ostringstream ss;
        ss << "Test field " << reinterpret_cast<const char *>(field.GetName().data);

        constexpr float FLOAT_VALUE = 1.0;
        constexpr double DOUBLE_VALUE = 2.0;
        int64_t value = 0;

        switch (field.GetTypeId()) {
            case panda_file::Type::TypeId::U1: {
                value = std::numeric_limits<uint8_t>::max();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I8: {
                value = std::numeric_limits<int8_t>::min();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U8: {
                value = std::numeric_limits<uint8_t>::max();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I16: {
                value = std::numeric_limits<int16_t>::min();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U16: {
                value = std::numeric_limits<uint16_t>::max();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I32: {
                value = std::numeric_limits<int32_t>::min();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U32: {
                value = std::numeric_limits<uint32_t>::max();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::I64: {
                value = std::numeric_limits<int64_t>::min();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::U64: {
                value = std::numeric_limits<uint64_t>::max();
                frameHandler.GetVReg(2).SetPrimitive(value);
                break;
            }
            case panda_file::Type::TypeId::F32: {
                frameHandler.GetVReg(2).SetPrimitive(FLOAT_VALUE);
                break;
            }
            case panda_file::Type::TypeId::F64: {
                frameHandler.GetVReg(2).SetPrimitive(DOUBLE_VALUE);
                break;
            }
            default: {
                UNREACHABLE();
                break;
            }
        }

        RuntimeInterface::SetupResolvedField(&field);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedField(nullptr);

        switch (field.GetTypeId()) {
            case panda_file::Type::TypeId::F32: {
                EXPECT_EQ(f->GetAccAsVReg().GetFloat(), FLOAT_VALUE) << ss.str();
                break;
            }
            case panda_file::Type::TypeId::F64: {
                EXPECT_EQ(f->GetAccAsVReg().GetDouble(), DOUBLE_VALUE) << ss.str();
                break;
            }
            default: {
                EXPECT_EQ(f->GetAccAsVReg().GetLong(), value) << ss.str();
                break;
            }
        }

        EXPECT_EQ(frameHandler.GetVReg(1).GetLong(), 0) << ss.str();
    }
}

void TestLoadStoreObjectField(bool isStatic)
{
    BytecodeEmitter emitter;

    std::ostringstream ss;
    ss << "Test load/store ";
    if (isStatic) {
        ss << "static ";
    }
    ss << "object field";

    if (isStatic) {
        emitter.LdstaticObj(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaObj(1);
        emitter.LdaObj(2);
        emitter.StstaticObj(RuntimeInterface::FIELD_ID.AsIndex());
        emitter.LdstaticObj(RuntimeInterface::FIELD_ID.AsIndex());
    } else {
        emitter.LdobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.StaObj(1);
        emitter.LdaObj(2);
        emitter.StobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
        emitter.LdobjObj(0, RuntimeInterface::FIELD_ID.AsIndex());
    }
    emitter.ReturnObj();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS) << ss.str();

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    pandasm::Parser p;
    std::string source;

    if (isStatic) {
        source = R"(
            .record R {
                R sf_ref <static>
            }
        )";
    } else {
        source = R"(
            .record R {
                R sf_ref
            }
        )";
    }

    auto res = p.Parse(source);
    auto classPf = pandasm::AsmEmitter::Emit(res.Value());

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    ASSERT_NE(classLinker, nullptr) << ss.str();

    classLinker->AddPandaFile(std::move(classPf));

    PandaString descriptor;

    auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    Class *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("R"), &descriptor));
    ASSERT_TRUE(classLinker->InitializeClass(ManagedThread::GetCurrent(), objectClass)) << ss.str();

    ObjectHeader *obj = nullptr;

    auto frameHandler = StaticFrameHandler(f.get());
    if (!isStatic) {
        obj = AllocObject(objectClass);
        frameHandler.GetVReg(0).SetReference(obj);
    }

    Span<Field> fields = isStatic ? objectClass->GetStaticFields() : objectClass->GetInstanceFields();
    Field *field = &fields[0];
    ObjectHeader *refValue = panda::mem::AllocateNullifiedPayloadString(1);

    frameHandler.GetVReg(2).SetReference(refValue);

    RuntimeInterface::SetupResolvedField(field);

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedField(nullptr);

    EXPECT_EQ(f->GetAccAsVReg().GetReference(), refValue) << ss.str();
    EXPECT_EQ(frameHandler.GetVReg(1).GetReference(), nullptr) << ss.str();
}

TEST_F(InterpreterTest, TestLoadStoreField)
{
    TestLoadStoreField(false);
    TestLoadStoreObjectField(false);
}

TEST_F(InterpreterTest, TestLoadStoreStaticField)
{
    TestLoadStoreField(true);
    TestLoadStoreObjectField(true);
}

TEST_F(InterpreterTest, TestReturns)
{
    int64_t value = 0xaabbccdd11223344;

    {
        BytecodeEmitter emitter;

        emitter.Return();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        f->GetAccAsVReg().SetPrimitive(value);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetAccAsVReg().Get(), static_cast<int32_t>(value));
        EXPECT_FALSE(f->GetAccAsVReg().HasObject());
    }

    {
        BytecodeEmitter emitter;

        emitter.ReturnWide();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        f->GetAccAsVReg().SetPrimitive(value);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetAccAsVReg().GetLong(), value);
        EXPECT_FALSE(f->GetAccAsVReg().HasObject());
    }

    {
        BytecodeEmitter emitter;

        emitter.ReturnObj();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());

        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        ObjectHeader *obj = panda::mem::AllocateNullifiedPayloadString(1);
        f->GetAccAsVReg().SetReference(obj);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        EXPECT_EQ(f->GetAccAsVReg().GetReference(), obj);
        EXPECT_TRUE(f->GetAccAsVReg().HasObject());
    }
}

auto AddProgramToClassLinker(std::string_view source, bool &failed)
{
    pandasm::Parser p;
    auto res = p.Parse(source.data());
    auto classPf = pandasm::AsmEmitter::Emit(res.Value());

    auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
    if (classLinker == nullptr) {
        failed = true;
        return classLinker;
    }

    classLinker->AddPandaFile(std::move(classPf));
    return classLinker;
}

auto InitObjectClass(ClassLinker *classLinker, const uint8_t *descriptor, bool &failed)
{
    auto *thread = ManagedThread::GetCurrent();
    auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *objectClass = ext->GetClass(descriptor);
    failed = !classLinker->InitializeClass(thread, objectClass);
    return objectClass;
}

auto GetClassDescriptor(const char *name)
{
    static PandaString descriptor;
    descriptor.clear();
    return ClassHelper::GetDescriptor(utf::CStringAsMutf8(name), &descriptor);
}

auto GetArrayDescriptor(const char *name, int rank)
{
    static PandaString descriptor;
    descriptor.clear();
    return ClassHelper::GetArrayDescriptor(utf::CStringAsMutf8(name), rank, &descriptor);
}

TEST_F(InterpreterTest, TestIsInstance)
{
    auto emitterFactory = []() {
        auto emitter = std::make_unique<BytecodeEmitter>();
        emitter->Isinstance(RuntimeInterface::TYPE_ID.AsIndex());
        emitter->Return();
        return emitter;
    };
    auto source = R"(
        .record R {}
    )";

    {
        auto emitter = emitterFactory();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);

        auto objectClass = InitObjectClass(classLinker.get(), GetClassDescriptor("R"), failed);
        ASSERT_FALSE(failed);

        f->GetAccAsVReg().SetReference(nullptr);

        RuntimeInterface::SetupResolvedClass(objectClass);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
        ASSERT_EQ(f->GetAccAsVReg().Get(), 0);
    }

    {
        auto emitter = emitterFactory();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetClassDescriptor("R"), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocObject(objectClass);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *dstClass = classLinker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
        ASSERT_TRUE(classLinker->InitializeClass(ManagedThread::GetCurrent(), dstClass));
        RuntimeInterface::SetupResolvedClass(dstClass);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
        ASSERT_EQ(f->GetAccAsVReg().Get(), 1);
    }

    {
        auto emitter = emitterFactory();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetArrayDescriptor("R", 2), failed);
        ASSERT_FALSE(failed);

        f->GetAccAsVReg().SetReference(nullptr);

        RuntimeInterface::SetupResolvedClass(objectClass);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
        ASSERT_EQ(f->GetAccAsVReg().Get(), 0);
    }

    {
        auto emitter = emitterFactory();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetArrayDescriptor("R", 2), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocArray(objectClass, sizeof(uint8_t), 0);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *dstClass = classLinker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
        ASSERT_TRUE(classLinker->InitializeClass(ManagedThread::GetCurrent(), dstClass));

        RuntimeInterface::SetupResolvedClass(dstClass);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
        ASSERT_EQ(f->GetAccAsVReg().Get(), 1);
    }
}

static ObjectHeader *CreateException(ManagedThread *thread)
{
    auto classLinker = CreateClassLinker(thread);
    auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto *cls = classLinker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
    auto *exception = ObjectHeader::Create(cls);
    return exception;
}

TEST_F(InterpreterTest, TestCheckCast)
{
    auto emitterFactory = []() {
        auto emitter = std::make_unique<BytecodeEmitter>();
        emitter->Checkcast(RuntimeInterface::TYPE_ID.AsIndex());
        emitter->ReturnVoid();
        return emitter;
    };

    {
        auto emitter = emitterFactory();
        auto source = R"(.record R {})";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetClassDescriptor("R"), failed);
        ASSERT_FALSE(failed);

        f->GetAccAsVReg().SetReference(nullptr);

        RuntimeInterface::SetupResolvedClass(objectClass);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
    }
    {
        auto emitter = emitterFactory();
        auto source = R"(.record R {})";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetArrayDescriptor("R", 2), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocArray(objectClass, sizeof(uint8_t), 0);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *dstClass = classLinker->GetExtension(ctx)->GetClassRoot(ClassRoot::OBJECT);
        ASSERT_TRUE(classLinker->InitializeClass(ManagedThread::GetCurrent(), dstClass));

        RuntimeInterface::SetupResolvedClass(dstClass);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
    }
    {
        auto emitter = emitterFactory();
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetClassDescriptor("C"), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocObject(objectClass);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        RuntimeInterface::SetupResolvedClass(classLinker->GetExtension(ctx)->GetClass(GetClassDescriptor("A")));
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
    }
    {
        auto emitter = emitterFactory();
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetArrayDescriptor("C", 2), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocArray(objectClass, sizeof(uint8_t), 0);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        RuntimeInterface::SetupResolvedClass(classLinker->GetExtension(ctx)->GetClass(GetArrayDescriptor("A", 2)));
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);
    }
    {
        BytecodeEmitter emitter;
        emitter.Checkcast(RuntimeInterface::TYPE_ID.AsIndex());
        emitter.ReturnVoid();

        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());

        emitter.ReturnObj();

        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        pandasm::Parser p;
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";

        auto res = p.Parse(source);
        auto classPf = pandasm::AsmEmitter::Emit(res.Value());

        auto classLinker = CreateClassLinker(ManagedThread::GetCurrent());
        ASSERT_NE(classLinker, nullptr);

        classLinker->AddPandaFile(std::move(classPf));

        PandaString descriptor;
        auto *thread = ManagedThread::GetCurrent();
        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *objectClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("A"), &descriptor));
        ASSERT_TRUE(classLinker->InitializeClass(thread, objectClass));

        auto *obj = AllocObject(objectClass);

        f->GetAccAsVReg().SetReference(obj);

        auto *dstClass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("C"), &descriptor));
        ASSERT_TRUE(classLinker->InitializeClass(thread, dstClass));
        RuntimeInterface::SetupResolvedClass(dstClass);

        RuntimeInterface::SetClassCastExceptionData({true, dstClass, objectClass});

        auto *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetupResolvedClass(nullptr);

        RuntimeInterface::SetClassCastExceptionData({false, nullptr, nullptr});

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAccAsVReg().GetReference(), exception);
    }
}

TEST_F(InterpreterTest, TestIsInstanceInheritance)
{
    auto emitterFactory = []() {
        auto emitter = std::make_unique<BytecodeEmitter>();
        emitter->Isinstance(RuntimeInterface::TYPE_ID.AsIndex());
        emitter->Return();
        return emitter;
    };

    {
        auto emitter = emitterFactory();
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetClassDescriptor("C"), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocObject(objectClass);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        RuntimeInterface::SetupResolvedClass(classLinker->GetExtension(ctx)->GetClass(GetClassDescriptor("A")));
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 1);
    }

    {
        auto emitter = emitterFactory();
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetArrayDescriptor("C", 2), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocArray(objectClass, sizeof(uint8_t), 0);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        RuntimeInterface::SetupResolvedClass(classLinker->GetExtension(ctx)->GetClass(GetArrayDescriptor("A", 2)));
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 1);
    }

    {
        auto emitter = emitterFactory();
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetClassDescriptor("A"), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocObject(objectClass);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        RuntimeInterface::SetupResolvedClass(classLinker->GetExtension(ctx)->GetClass(GetClassDescriptor("C")));
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 0);
    }

    {
        auto emitter = emitterFactory();
        auto source = R"(
            .record A {}
            .record B <extends=A> {}
            .record C <extends=B> {}
        )";
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter->Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);
        auto objectClass = InitObjectClass(classLinker.get(), GetArrayDescriptor("A", 2), failed);
        ASSERT_FALSE(failed);

        auto *obj = AllocArray(objectClass, sizeof(uint8_t), 0);
        f->GetAccAsVReg().SetReference(obj);

        auto ctx = Runtime::GetCurrent()->GetLanguageContext(panda_file::SourceLang::PANDA_ASSEMBLY);
        RuntimeInterface::SetupResolvedClass(classLinker->GetExtension(ctx)->GetClass(GetArrayDescriptor("C", 2)));
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedClass(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 0);
    }
}

TEST_F(InterpreterTest, TestVirtualCalls)
{
    {
        auto emitter = BytecodeEmitter {};
        emitter.CallVirtShort(0, 1, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        auto source = R"(
            .record A {}
            .record B <extends=A> {}

            .function i32 A.foo(A a0, i32 a1) {
                lda a1
                addi 1
                return
            }

            .function i32 B.foo(B a0, i32 a1) {
                lda a1
                addi 2
                return
            }
        )";

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);

        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *classA = ext->GetClass(GetClassDescriptor("A"));
        auto *objectClass = ext->GetClass(GetClassDescriptor("B"));
        auto *callee = classA->GetMethods().data();
        auto *obj = AllocObject(objectClass);

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).SetReference(obj);
        frameHandler.GetVReg(1).Set(1);

        RuntimeInterface::SetupResolvedMethod(callee);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedMethod(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 3);
    }

    {
        auto emitter = BytecodeEmitter {};
        emitter.CallVirt(0, 1, 2, 3, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        auto source = R"(
            .record A {}
            .record B <extends=A> {}

            .function i32 A.foo(A a0, i32 a1, i32 a2, i32 a3) {
                lda a1
                addi 1
                return
            }

            .function i32 B.foo(B a0, i32 a1, i32 a2, i32 a3) {
                lda a1
                addi 2
                add2 a2
                add2 a3
                return
            }
        )";

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);

        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *classA = ext->GetClass(GetClassDescriptor("A"));
        auto *objectClass = ext->GetClass(GetClassDescriptor("B"));
        auto *callee = classA->GetMethods().data();
        auto *obj = AllocObject(objectClass);

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).SetReference(obj);
        frameHandler.GetVReg(1).Set(1);
        frameHandler.GetVReg(2).Set(2);
        frameHandler.GetVReg(3).Set(3);

        RuntimeInterface::SetupResolvedMethod(callee);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedMethod(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 8);
    }

    {
        auto emitter = BytecodeEmitter {};
        emitter.CallVirtRange(0, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        auto source = R"(
            .record A {}
            .record B <extends=A> {}

            .function i32 A.foo(A a0, i32 a1, i32 a2, i32 a3, i32 a4) {
                lda a1
                addi 1
                return
            }

            .function i32 B.foo(B a0, i32 a1, i32 a2, i32 a3, i32 a4) {
                lda a1
                addi 2
                add2 a2
                add2 a3
                add2 a4
                return
            }
        )";

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);

        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *classA = ext->GetClass(GetClassDescriptor("A"));
        auto *objectClass = ext->GetClass(GetClassDescriptor("B"));
        auto *callee = classA->GetMethods().data();
        auto *obj = AllocObject(objectClass);

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).SetReference(obj);
        frameHandler.GetVReg(1).Set(1);
        frameHandler.GetVReg(2).Set(2);
        frameHandler.GetVReg(3).Set(3);
        frameHandler.GetVReg(4).Set(4);

        RuntimeInterface::SetupResolvedMethod(callee);
        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());
        RuntimeInterface::SetupResolvedMethod(nullptr);

        ASSERT_EQ(f->GetAccAsVReg().Get(), 12);
    }
}

TEST_F(InterpreterTest, TestVirtualCallsExceptions)
{
    auto testNullReferenceException = [](BytecodeEmitter &emitter) {
        std::vector<uint8_t> bytecode;
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        RuntimeInterface::SetCatchBlockPcOffset(bytecode.size());
        emitter.ReturnObj();
        ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

        auto f = CreateFrame(16, nullptr, nullptr);
        InitializeFrame(f.get());
        auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto methodData = CreateMethod(cls, f.get(), bytecode);
        auto method = std::move(methodData.first);
        f->SetMethod(method.get());

        auto source = R"(
            .record A {}
            .record B <extends=A> {}

            .function i32 A.foo(A a0) {
                ldai 0
                return
            }

            .function i32 B.foo(B a0) {
                ldai 1
                return
            }
        )";

        bool failed = false;
        auto classLinker = AddProgramToClassLinker(source, failed);
        ASSERT_FALSE(failed);

        auto *ext = classLinker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto *classA = ext->GetClass(GetClassDescriptor("A"));
        auto *callee = classA->GetMethods().data();

        auto frameHandler = StaticFrameHandler(f.get());
        frameHandler.GetVReg(0).SetReference(nullptr);

        RuntimeInterface::SetupResolvedMethod(callee);
        RuntimeInterface::SetNullPointerExceptionData({true});

        auto *thread = ManagedThread::GetCurrent();
        auto *exception = CreateException(thread);
        thread->SetException(exception);

        Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

        RuntimeInterface::SetNullPointerExceptionData({false});
        RuntimeInterface::SetupResolvedMethod(nullptr);

        ASSERT_FALSE(thread->HasPendingException());
        ASSERT_EQ(f->GetAccAsVReg().GetReference(), exception);
    };

    {
        auto emitter = BytecodeEmitter {};
        emitter.CallVirtShort(0, 0, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();
        testNullReferenceException(emitter);
    }

    {
        auto emitter = BytecodeEmitter {};
        emitter.CallVirt(0, 0, 0, 0, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();
        testNullReferenceException(emitter);
    }

    {
        auto emitter = BytecodeEmitter {};
        emitter.CallVirtRange(0, RuntimeInterface::METHOD_ID.AsIndex());
        emitter.Return();
        testNullReferenceException(emitter);
    }
}

static void MakeShorty(size_t numArgs, std::vector<uint16_t> *buf)
{
    static constexpr auto I64 = static_cast<uint8_t>(panda_file::Type::TypeId::I64);
    static constexpr size_t ELEM_SIZE = 4;
    static constexpr size_t ELEM_COUNT = std::numeric_limits<uint16_t>::digits / ELEM_SIZE;

    uint16_t val = 0;
    uint32_t count = 1;
    ++numArgs;  // consider the return value
    while (numArgs > 0) {
        if (count == ELEM_COUNT) {
            buf->push_back(val);
            val = 0;
            count = 0;
        }
        // NOLINTNEXTLINE(hicpp-signed-bitwise)
        val |= I64 << ELEM_SIZE * count;
        ++count;
        --numArgs;
    }
    if (count == ELEM_COUNT) {
        buf->push_back(val);
        val = 0;
    }
    buf->push_back(val);
}

template <bool IS_DYNAMIC = false>
static std::pair<PandaUniquePtr<Method>, std::unique_ptr<const panda_file::File>> CreateResolvedMethod(
    Class *klass, size_t vregNum, const std::vector<int64_t> &args, std::vector<uint8_t> *bytecode,
    std::vector<uint16_t> *shortyBuf)
{
    BytecodeEmitter emitter;
    Label label = emitter.CreateLabel();

    size_t startIdx = 0;
    if constexpr (IS_DYNAMIC) {
        ++startIdx;  // skip function object
    }
    for (size_t i = startIdx; i < args.size(); i++) {
        emitter.LdaiWide(args[i]);
        emitter.Jne(vregNum + i, label);
    }

    emitter.LdaiWide(1);
    emitter.ReturnWide();
    emitter.Bind(label);
    emitter.LdaiWide(0);
    emitter.ReturnWide();

    [[maybe_unused]] auto res = emitter.Build(&*bytecode);

    ASSERT(res == BytecodeEmitter::ErrorCode::SUCCESS);

    MakeShorty(args.size(), shortyBuf);

    return CreateMethod(klass, 0, args.size(), vregNum, shortyBuf->data(), *bytecode);
}

CompilerInterface::ReturnReason EntryPoint(CompilerInterface::ExecState *state)
{
    ASSERT(state->GetNumArgs() == 2);

    ASSERT(state->GetFrame()->GetSize() == 16);

    [[maybe_unused]] auto frameHandler = StaticFrameHandler(state->GetFrame());
    ASSERT(frameHandler.GetVReg(1).GetLong() == 1);
    ASSERT(frameHandler.GetVReg(3).GetLong() == 2);

    ASSERT(frameHandler.GetVReg(2).GetLong() == 3);
    ASSERT(frameHandler.GetVReg(4).GetLong() == 4);

    int64_t v = 100 + state->GetArg(0).GetLong() + state->GetArg(1).GetLong();
    interpreter::VRegister acc;
    acc.Set(0);
    acc.Set(v);
    state->SetAcc(acc);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    state->SetPc(state->GetPc() + 1);

    return CompilerInterface::ReturnReason::RET_OK;
}

// NOTE (Baladurin, Udovichenko) change for new interop
TEST_F(InterpreterTest, DISABLED_TestCallNative)
{
    size_t vregNum = 10;

    BytecodeEmitter emitter;

    // first call hotness_counter is 0
    emitter.CallShort(1, 3, RuntimeInterface::METHOD_ID.AsIndex());
    emitter.StaWide(5);
    // second call hotness counter is 1 -> native call
    emitter.CallShort(5, 6, RuntimeInterface::METHOD_ID.AsIndex());
    // native call skips this return
    emitter.ReturnWide();
    emitter.Addi(1);
    emitter.ReturnWide();

    std::vector<uint8_t> bytecode;
    ASSERT_EQ(emitter.Build(&bytecode), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f = CreateFrame(16, nullptr, nullptr);
    InitializeFrame(f.get());

    auto frameHandler = StaticFrameHandler(f.get());
    std::vector<int64_t> args1 = {1, 2};
    frameHandler.GetVReg(1).SetPrimitive(args1[0]);
    frameHandler.GetVReg(3).SetPrimitive(args1[1]);

    std::vector<int64_t> args2 = {3, 4};
    frameHandler.GetVReg(2).SetPrimitive(args2[0]);
    frameHandler.GetVReg(4).SetPrimitive(args2[1]);

    auto cls = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData = CreateMethod(cls, f.get(), bytecode);
    auto method = std::move(methodData.first);
    f->SetMethod(method.get());

    std::vector<uint16_t> shortyBuf;
    std::vector<uint8_t> methodBytecode;
    auto resolvedMethodData = CreateResolvedMethod(cls, vregNum, args1, &methodBytecode, &shortyBuf);
    auto resolvedMethod = std::move(resolvedMethodData.first);

    RuntimeInterface::SetCompilerHotnessThreshold(1);

    RuntimeInterface::SetupNativeEntryPoint(reinterpret_cast<const void *>(EntryPoint));

    RuntimeInterface::SetupResolvedMethod(resolvedMethod.get());

    Execute(ManagedThread::GetCurrent(), bytecode.data(), f.get());

    RuntimeInterface::SetupResolvedMethod(nullptr);

    EXPECT_EQ(f->GetAccAsVReg().GetLong(), 102);
}

class InterpreterWithSTWTest : public testing::Test {
public:
    InterpreterWithSTWTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        options.SetRunGcInPlace(true);
        options.SetGcTriggerType("debug-never");
        options.SetVerifyCallStack(false);
        options.SetGcType("stw");
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~InterpreterWithSTWTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    NO_COPY_SEMANTIC(InterpreterWithSTWTest);
    NO_MOVE_SEMANTIC(InterpreterWithSTWTest);

private:
    panda::MTManagedThread *thread_;
};

Frame *CreateFrameWithSize(uint32_t size, uint32_t nregs, Method *method, Frame *prev, ManagedThread *current)
{
    uint32_t extSz = CORE_EXT_FRAME_DATA_SIZE;
    size_t allocSz = Frame::GetAllocSize(size, extSz);
    size_t mirrorOffset = extSz + sizeof(Frame) + sizeof(interpreter::VRegister) * nregs;
    void *frame = current->GetStackFrameAllocator()->Alloc<false>(allocSz);
    auto mirrorPartBytes = reinterpret_cast<uint64_t *>(ToVoidPtr(ToUintPtr(frame) + mirrorOffset));
    for (size_t i = 0; i < nregs; i++) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        mirrorPartBytes[i] = 0x00;
    }
    return new (Frame::FromExt(frame, extSz)) Frame(frame, method, prev, nregs);
}

#if defined(PANDA_TARGET_ARM32) && defined(NDEBUG)
DEATH_TEST_F(InterpreterWithSTWTest, DISABLED_TestCreateFrame)
#else
DEATH_TEST_F(InterpreterWithSTWTest, TestCreateFrame)
#endif
{
    testing::FLAGS_gtest_death_test_style = "threadsafe";

    size_t vregNum1 = 16;

    BytecodeEmitter emitter1;

    emitter1.CallShort(1, 3, RuntimeInterface::METHOD_ID.AsIndex());
    emitter1.ReturnWide();

    std::vector<uint8_t> bytecode1;
    ASSERT_EQ(emitter1.Build(&bytecode1), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f1 = CreateFrame(vregNum1, nullptr, nullptr);

    auto cls1 = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData1 = CreateMethod(cls1, f1.get(), bytecode1);
    auto method1 = std::move(methodData1.first);
    f1->SetMethod(method1.get());

    auto frameHandler1 = StaticFrameHandler(f1.get());
    frameHandler1.GetVReg(1).SetPrimitive(1);
    frameHandler1.GetVReg(3).SetPrimitive(2);

    size_t vregNum2 = 65535;

    BytecodeEmitter emitter2;

    emitter2.LdaObj(1);
    emitter2.ReturnObj();

    std::vector<uint8_t> bytecode2;
    ASSERT_EQ(emitter2.Build(&bytecode2), BytecodeEmitter::ErrorCode::SUCCESS);

    auto f2 = CreateFrameWithSize(Frame::GetActualSize<false>(vregNum2), vregNum2, method1.get(), f1.get(),
                                  ManagedThread::GetCurrent());

    auto cls2 = CreateClass(panda_file::SourceLang::PANDA_ASSEMBLY);
    auto methodData2 = CreateMethod(cls2, f2, bytecode2);
    auto method2 = std::move(methodData2.first);
    f2->SetMethod(method2.get());
    ManagedThread::GetCurrent()->SetCurrentFrame(f2);

    auto frameHandler2 = StaticFrameHandler(f2);
    for (size_t i = 0; i < vregNum2; i++) {
        frameHandler2.GetVReg(i).SetReference(panda::mem::AllocNonMovableObject());
    }

    size_t extSz = CORE_EXT_FRAME_DATA_SIZE;
    size_t allocSz = sizeof(interpreter::VRegister) * vregNum2;
    memset_s(ToVoidPtr(ToUintPtr(f2) + extSz + sizeof(Frame)), allocSz, 0xff, allocSz);

    panda::mem::GC *gc = Runtime::GetCurrent()->GetPandaVM()->GetGC();

    {
        ScopedNativeCodeThread sn(ManagedThread::GetCurrent());
        GCTask task(GCTaskCause::OOM_CAUSE);
        ASSERT_DEATH(task.Run(*gc), "");
    }

    panda::FreeFrameInterp(f2, ManagedThread::GetCurrent());

    f2 = CreateFrameWithSize(Frame::GetActualSize<false>(vregNum2), vregNum2, method1.get(), f1.get(),
                             ManagedThread::GetCurrent());

    {
        ScopedNativeCodeThread sn(ManagedThread::GetCurrent());
        GCTask task(GCTaskCause::OOM_CAUSE);
        task.Run(*gc);
    }

    panda::FreeFrameInterp(f2, ManagedThread::GetCurrent());
}

}  // namespace panda::interpreter::test

// NOLINTEND(readability-magic-numbers)
