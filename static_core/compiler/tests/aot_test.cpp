/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "unit_test.h"
#include "aot/aot_manager.h"
#include "aot/aot_builder/aot_builder.h"
#include "aot/compiled_method.h"
#include "compiler/code_info/code_info_builder.h"
#include "os/exec.h"
#include "assembly-parser.h"
#include <elf.h>
#include "utils/string_helpers.h"
#include "events/events.h"
#include "mem/gc/gc_types.h"
#include "runtime/include/file_manager.h"

#include <regex>

using panda::panda_file::File;

namespace panda::compiler {
class AotTest : public AsmTest {
public:
    AotTest()
    {
        std::string exePath = GetExecPath();
        auto pos = exePath.rfind('/');
        paocPath_ = exePath.substr(0U, pos) + "/../bin/ark_aot";
        aotdumpPath_ = exePath.substr(0U, pos) + "/../bin/ark_aotdump";
    }

    ~AotTest() override = default;

    NO_MOVE_SEMANTIC(AotTest);
    NO_COPY_SEMANTIC(AotTest);

    const char *GetPaocFile() const
    {
        return paocPath_.c_str();
    }

    const char *GetAotdumpFile() const
    {
        return aotdumpPath_.c_str();
    }

    std::string GetPaocDirectory() const
    {
        auto pos = paocPath_.rfind('/');
        return paocPath_.substr(0U, pos);
    }

    const char *GetArchAsArgString() const
    {
        switch (targetArch_) {
            case Arch::AARCH32:
                return "arm";
            case Arch::AARCH64:
                return "arm64";
            case Arch::X86:
                return "x86";
            case Arch::X86_64:
                return "x86_64";
            default:
                UNREACHABLE();
        }
    }

    void RunAotdump(const std::string &aotFilename)
    {
        TmpFile tmpfile("aotdump.tmp");

        auto res = os::exec::Exec(GetAotdumpFile(), "--show-code=disasm", "--output-file", tmpfile.GetFileName(),
                                  aotFilename.c_str());
        ASSERT_TRUE(res) << "aotdump failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U) << "aotdump return error code: " << res.Value();
    }

private:
    Arch targetArch_ = Arch::AARCH64;
    std::string paocPath_;
    std::string aotdumpPath_;
};

// NOLINTBEGIN(readability-magic-numbers)
#ifdef PANDA_COMPILER_TARGET_AARCH64
TEST_F(AotTest, PaocBootPandaFiles)
{
    // Test basic functionality only in host mode.
    if (RUNTIME_ARCH != Arch::X86_64) {
        return;
    }
    TmpFile pandaFname("test.pf");
    TmpFile aotFname("./test.an");
    static const std::string LOCATION = "/data/local/tmp";
    static const std::string PANDA_FILE_PATH = LOCATION + "/" + pandaFname.GetFileName();

    auto source = R"(
        .function void dummy() {
            return.void
        }
    )";

    {
        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname.GetFileName(), res.Value()));
    }

    // Correct path to arkstdlib.abc
    {
        auto pandastdlibPath = GetPaocDirectory() + "/../pandastdlib/arkstdlib.abc";
        auto res = os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname.GetFileName(), "--paoc-output",
                                  aotFname.GetFileName(), "--paoc-location", LOCATION.c_str(), "--compiler-cross-arch",
                                  GetArchAsArgString(), "--boot-panda-files", pandastdlibPath.c_str());
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U) << "Aot compiler failed with code " << res.Value();
        RunAotdump(aotFname.GetFileName());
    }
}

TEST_F(AotTest, PaocLocation)
{
    // Test basic functionality only in host mode.
    if (RUNTIME_ARCH != Arch::X86_64) {
        return;
    }
    TmpFile pandaFname("test.pf");
    TmpFile aotFname("./test.an");
    static const std::string LOCATION = "/data/local/tmp";
    static const std::string PANDA_FILE_PATH = LOCATION + "/" + pandaFname.GetFileName();

    auto source = R"(
        .function u32 add(u64 a0, u64 a1) {
            add a0, a1
            return
        }
    )";

    {
        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname.GetFileName(), res.Value()));
    }

    {
        auto pandastdlibPath = GetPaocDirectory() + "/../pandastdlib/arkstdlib.abc";
        auto res = os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname.GetFileName(), "--paoc-output",
                                  aotFname.GetFileName(), "--paoc-location", LOCATION.c_str(),
                                  "--compiler-cross-arch=x86_64", "--gc-type=epsilon", "--paoc-use-cha=false");
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U) << "Aot compiler failed with code " << res.Value();
    }

    AotManager aotManager;
    {
        auto res = aotManager.AddFile(aotFname.GetFileName(), nullptr, static_cast<uint32_t>(mem::GCType::EPSILON_GC));
        ASSERT_TRUE(res) << res.Error();
    }

    auto aotFile = aotManager.GetFile(aotFname.GetFileName());
    ASSERT_TRUE(aotFile);
    ASSERT_EQ(aotFile->GetFilesCount(), 1U);
    ASSERT_TRUE(aotFile->FindPandaFile(PANDA_FILE_PATH));
}
#endif  // PANDA_COMPILER_TARGET_AARCH64

TEST_F(AotTest, BuildAndLoad)
{
    if (RUNTIME_ARCH == Arch::AARCH32) {
        // NOTE(msherstennikov): for some reason dlopen cannot open aot file in qemu-arm
        return;
    }
    uint32_t tid = os::thread::GetCurrentThreadId();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::string tmpfile = helpers::string::Format("/tmp/tmpfile_%04x.pn", tid);
    const char *tmpfilePn = tmpfile.c_str();
    static constexpr const char *TMPFILE_PF = "test.pf";
    static constexpr const char *CMDLINE = "cmdline";
    static constexpr uint32_t METHOD1_ID = 42;
    static constexpr uint32_t METHOD2_ID = 43;
    const std::string className("Foo");
    std::string methodName(className + "::method");
    std::array<uint8_t, 4U> x86Add = {
        0x8dU, 0x04U, 0x37U,  // lea    eax,[rdi+rdi*1]
        0xc3U                 // ret
    };

    AotBuilder aotBuilder;
    aotBuilder.SetArch(RUNTIME_ARCH);
    aotBuilder.SetGcType(static_cast<uint32_t>(mem::GCType::STW_GC));
    RuntimeInterfaceMock iruntime;
    aotBuilder.SetRuntime(&iruntime);

    aotBuilder.StartFile(TMPFILE_PF, 0x12345678U);

    auto thread = MTManagedThread::GetCurrent();
    if (thread != nullptr) {
        thread->ManagedCodeBegin();
    }
    auto runtime = Runtime::GetCurrent();
    auto etx = runtime->GetClassLinker()->GetExtension(runtime->GetLanguageContext(runtime->GetRuntimeType()));
    auto klass = etx->CreateClass(reinterpret_cast<const uint8_t *>(className.data()), 0, 0,
                                  AlignUp(sizeof(Class), OBJECT_POINTER_SIZE));
    if (thread != nullptr) {
        thread->ManagedCodeEnd();
    }

    klass->SetFileId(panda_file::File::EntityId(13U));
    aotBuilder.StartClass(*klass);

    Method method1(klass, nullptr, File::EntityId(METHOD1_ID), File::EntityId(), 0U, 1U, nullptr);
    {
        CodeInfoBuilder codeBuilder(RUNTIME_ARCH, GetAllocator());
        ArenaVector<uint8_t> data(GetAllocator()->Adapter());
        codeBuilder.Encode(&data);
        CompiledMethod compiledMethod1(RUNTIME_ARCH, &method1, 0U);
        compiledMethod1.SetCode(Span(reinterpret_cast<const uint8_t *>(methodName.data()), methodName.size() + 1U));
        compiledMethod1.SetCodeInfo(Span(data).ToConst());
        aotBuilder.AddMethod(compiledMethod1);
    }

    Method method2(klass, nullptr, File::EntityId(METHOD2_ID), File::EntityId(), 0U, 1U, nullptr);
    {
        CodeInfoBuilder codeBuilder(RUNTIME_ARCH, GetAllocator());
        ArenaVector<uint8_t> data(GetAllocator()->Adapter());
        codeBuilder.Encode(&data);
        CompiledMethod compiledMethod2(RUNTIME_ARCH, &method2, 1U);
        compiledMethod2.SetCode(Span(reinterpret_cast<const uint8_t *>(x86Add.data()), x86Add.size()));
        compiledMethod2.SetCodeInfo(Span(data).ToConst());
        aotBuilder.AddMethod(compiledMethod2);
    }

    aotBuilder.EndClass();
    uint32_t hash = GetHash32String(reinterpret_cast<const uint8_t *>(className.data()));
    aotBuilder.InsertEntityPairHeader(hash, 13U);
    aotBuilder.InsertClassHashTableSize(1U);
    aotBuilder.EndFile();

    aotBuilder.Write(CMDLINE, tmpfilePn);

    AotManager aotManager;
    auto res = aotManager.AddFile(tmpfilePn, nullptr, static_cast<uint32_t>(mem::GCType::STW_GC));
    ASSERT_TRUE(res) << res.Error();

    auto aotFile = aotManager.GetFile(tmpfilePn);
    ASSERT_TRUE(aotFile);
    ASSERT_TRUE(strcmp(CMDLINE, aotFile->GetCommandLine()) == 0U);
    ASSERT_TRUE(strcmp(tmpfilePn, aotFile->GetFileName()) == 0U);
    ASSERT_EQ(aotFile->GetFilesCount(), 1U);

    auto pfile = aotManager.FindPandaFile(TMPFILE_PF);
    ASSERT_NE(pfile, nullptr);
    auto cls = pfile->GetClass(13U);
    ASSERT_TRUE(cls.IsValid());

    {
        auto code = cls.FindMethodCodeEntry(0U);
        ASSERT_FALSE(code == nullptr);
        ASSERT_EQ(methodName, reinterpret_cast<const char *>(code));
    }

    {
        auto code = cls.FindMethodCodeEntry(1U);
        ASSERT_FALSE(code == nullptr);
        ASSERT_EQ(std::memcmp(x86Add.data(), code, x86Add.size()), 0U);
#ifdef PANDA_TARGET_AMD64
        auto funcAdd = reinterpret_cast<int (*)(int, int)>(const_cast<void *>(code));
        ASSERT_EQ(funcAdd(2U, 3U), 5U);
#endif
    }
}

TEST_F(AotTest, PaocSpecifyMethods)
{
#ifndef PANDA_EVENTS_ENABLED
    GTEST_SKIP();
#endif

    // Test basic functionality only in host mode.
    if (RUNTIME_ARCH != Arch::X86_64) {
        return;
    }
    TmpFile pandaFname("test.pf");
    TmpFile paocOutputName("events-out.csv");

    static const std::string LOCATION = "/data/local/tmp";
    static const std::string PANDA_FILE_PATH = LOCATION + "/" + pandaFname.GetFileName();

    auto source = R"(
        .record A {}
        .record B {}

        .function i32 A.f1() {
            ldai 10
            return
        }

        .function i32 B.f1() {
            ldai 20
            return
        }

        .function i32 A.f2() {
            ldai 10
            return
        }

        .function i32 B.f2() {
            ldai 20
            return
        }

        .function i32 main() {
            ldai 0
            return
        }
    )";

    {
        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname.GetFileName(), res.Value()));
    }

    {
        // paoc will try compiling all the methods from the panda-file that matches `--compiler-regex`
        auto res =
            os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname.GetFileName(), "--compiler-regex", "B::f1",
                           "--paoc-mode=jit", "--events-output=csv", "--events-file", paocOutputName.GetFileName());
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U);

        std::ifstream infile(paocOutputName.GetFileName());
        std::regex rgx("Compilation,B::f1.*,COMPILED");
        for (std::string line; std::getline(infile, line);) {
            if (line.rfind("Compilation", 0U) == 0U) {
                ASSERT_TRUE(std::regex_match(line, rgx));
            }
        }
    }
}

TEST_F(AotTest, PaocMultipleFiles)
{
    if (RUNTIME_ARCH != Arch::X86_64) {
        GTEST_SKIP();
    }

    TmpFile aotFname("./test.an");
    TmpFile pandaFname1("test1.pf");
    TmpFile pandaFname2("test2.pf");

    {
        auto source = R"(
            .function f64 main() {
                fldai.64 3.1415926
                return.64
            }
        )";

        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname1.GetFileName(), res.Value()));
    }

    {
        auto source = R"(
            .record MyMath {
            }

            .function f64 MyMath.getPi() <static> {
                fldai.64 3.1415926
                return.64
            }
        )";

        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname2.GetFileName(), res.Value()));
    }

    {
        std::stringstream pandaFiles;
        pandaFiles << pandaFname1.GetFileName() << ',' << pandaFname2.GetFileName();
        auto res = os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFiles.str().c_str(), "--paoc-output",
                                  aotFname.GetFileName(), "--gc-type=epsilon", "--paoc-use-cha=false");
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U);
    }

    {
        AotManager aotManager;
        auto res = aotManager.AddFile(aotFname.GetFileName(), nullptr, static_cast<uint32_t>(mem::GCType::EPSILON_GC));
        ASSERT_TRUE(res) << res.Error();

        auto aotFile = aotManager.GetFile(aotFname.GetFileName());
        ASSERT_TRUE(aotFile);
        ASSERT_EQ(aotFile->GetFilesCount(), 2U);
    }
    RunAotdump(aotFname.GetFileName());
}

TEST_F(AotTest, PaocGcType)
{
    if (RUNTIME_ARCH != Arch::X86_64) {
        GTEST_SKIP();
    }

    TmpFile aotFname("./test.pn");
    TmpFile pandaFname("test.pf");

    {
        auto source = R"(
            .function f64 main() {
                fldai.64 3.1415926
                return.64
            }
        )";

        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname.GetFileName(), res.Value()));
    }

    {
        auto res = os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname.GetFileName(), "--paoc-output",
                                  aotFname.GetFileName(), "--gc-type=epsilon", "--paoc-use-cha=false");
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U);
    }

    {
        // Wrong gc-type
        AotManager aotManager;
        auto res = aotManager.AddFile(aotFname.GetFileName(), nullptr, static_cast<uint32_t>(mem::GCType::STW_GC));
        ASSERT_FALSE(res) << res.Error();
        std::string expectedString = "Wrong AotHeader gc-type: epsilon vs stw";
        ASSERT_NE(res.Error().find(expectedString), std::string::npos);
    }

    {
        AotManager aotManager;
        auto res = aotManager.AddFile(aotFname.GetFileName(), nullptr, static_cast<uint32_t>(mem::GCType::EPSILON_GC));
        ASSERT_TRUE(res) << res.Error();

        auto aotFile = aotManager.GetFile(aotFname.GetFileName());
        ASSERT_TRUE(aotFile);
        ASSERT_EQ(aotFile->GetFilesCount(), 1U);
    }
    RunAotdump(aotFname.GetFileName());
}

TEST_F(AotTest, FileManagerLoadAbc)
{
    if (RUNTIME_ARCH != Arch::X86_64) {
        GTEST_SKIP();
    }

    TmpFile aotFname("./test.an");
    TmpFile pandaFname("./test.pf");

    {
        auto source = R"(
            .function f64 main() {
                fldai.64 3.1415926
                return.64
            }
        )";

        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname.GetFileName(), res.Value()));
    }

    {
        auto runtime = Runtime::GetCurrent();
        auto gcType = Runtime::GetGCType(runtime->GetOptions(), plugins::RuntimeTypeToLang(runtime->GetRuntimeType()));
        auto gcTypeName = "--gc-type=epsilon";
        if (gcType == mem::GCType::STW_GC) {
            gcTypeName = "--gc-type=stw";
        } else if (gcType == mem::GCType::GEN_GC) {
            gcTypeName = "--gc-type=gen-gc";
        } else {
            ASSERT_TRUE(gcType == mem::GCType::EPSILON_GC || gcType == mem::GCType::EPSILON_G1_GC)
                << "Invalid GC type\n";
        }
        auto res = os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname.GetFileName(), "--paoc-output",
                                  aotFname.GetFileName(), gcTypeName, "--paoc-use-cha=false");
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U);
    }

    {
        auto res = FileManager::LoadAbcFile(pandaFname.GetFileName(), panda_file::File::READ_ONLY);
        ASSERT_TRUE(res);
        auto aotManager = Runtime::GetCurrent()->GetClassLinker()->GetAotManager();
        auto aotFile = aotManager->GetFile(aotFname.GetFileName());
        ASSERT_TRUE(aotFile);
        ASSERT_EQ(aotFile->GetFilesCount(), 1U);
    }
    RunAotdump(aotFname.GetFileName());
}

TEST_F(AotTest, FileManagerLoadAn)
{
    if (RUNTIME_ARCH == Arch::AARCH32) {
        // NOTE(msherstennikov): for some reason dlopen cannot open aot file in qemu-arm
        return;
    }
    uint32_t tid = os::thread::GetCurrentThreadId();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::string tmpfile = helpers::string::Format("test.an", tid);
    const char *tmpfilePn = tmpfile.c_str();
    static constexpr const char *TMPFILE_PF = "test.pf";
    static constexpr const char *CMDLINE = "cmdline";
    static constexpr uint32_t METHOD1_ID = 42;
    static constexpr uint32_t METHOD2_ID = 43;
    const std::string className("Foo");
    std::string methodName(className + "::method");
    std::array<uint8_t, 4U> x86Add = {
        0x8dU, 0x04U, 0x37U,  // lea    eax,[rdi+rdi*1]
        0xc3U                 // ret
    };

    AotBuilder aotBuilder;
    aotBuilder.SetArch(RUNTIME_ARCH);
    RuntimeInterfaceMock iruntime;
    aotBuilder.SetRuntime(&iruntime);
    auto runtime = Runtime::GetCurrent();
    auto gcType = Runtime::GetGCType(runtime->GetOptions(), plugins::RuntimeTypeToLang(runtime->GetRuntimeType()));
    aotBuilder.SetGcType(static_cast<uint32_t>(gcType));

    aotBuilder.StartFile(TMPFILE_PF, 0x12345678U);

    auto thread = MTManagedThread::GetCurrent();
    if (thread != nullptr) {
        thread->ManagedCodeBegin();
    }
    auto etx = runtime->GetClassLinker()->GetExtension(runtime->GetLanguageContext(runtime->GetRuntimeType()));
    auto klass = etx->CreateClass(reinterpret_cast<const uint8_t *>(className.data()), 0, 0,
                                  AlignUp(sizeof(Class), OBJECT_POINTER_SIZE));
    if (thread != nullptr) {
        thread->ManagedCodeEnd();
    }

    klass->SetFileId(panda_file::File::EntityId(13U));
    aotBuilder.StartClass(*klass);

    Method method1(klass, nullptr, File::EntityId(METHOD1_ID), File::EntityId(), 0U, 1U, nullptr);
    {
        CodeInfoBuilder codeBuilder(RUNTIME_ARCH, GetAllocator());
        ArenaVector<uint8_t> data(GetAllocator()->Adapter());
        codeBuilder.Encode(&data);
        CompiledMethod compiledMethod1(RUNTIME_ARCH, &method1, 0U);
        compiledMethod1.SetCode(Span(reinterpret_cast<const uint8_t *>(methodName.data()), methodName.size() + 1U));
        compiledMethod1.SetCodeInfo(Span(data).ToConst());
        aotBuilder.AddMethod(compiledMethod1);
    }

    Method method2(klass, nullptr, File::EntityId(METHOD2_ID), File::EntityId(), 0U, 1U, nullptr);
    {
        CodeInfoBuilder codeBuilder(RUNTIME_ARCH, GetAllocator());
        ArenaVector<uint8_t> data(GetAllocator()->Adapter());
        codeBuilder.Encode(&data);
        CompiledMethod compiledMethod2(RUNTIME_ARCH, &method2, 1U);
        compiledMethod2.SetCode(Span(reinterpret_cast<const uint8_t *>(x86Add.data()), x86Add.size()));
        compiledMethod2.SetCodeInfo(Span(data).ToConst());
        aotBuilder.AddMethod(compiledMethod2);
    }

    aotBuilder.EndClass();
    uint32_t hash = GetHash32String(reinterpret_cast<const uint8_t *>(className.data()));
    aotBuilder.InsertEntityPairHeader(hash, 13U);
    aotBuilder.InsertClassHashTableSize(1U);
    aotBuilder.EndFile();

    aotBuilder.Write(CMDLINE, tmpfilePn);
    {
        auto res = FileManager::LoadAnFile(tmpfilePn);
        ASSERT_TRUE(res) << "Fail to load an file";
    }

    auto aotManager = Runtime::GetCurrent()->GetClassLinker()->GetAotManager();
    auto aotFile = aotManager->GetFile(tmpfilePn);
    ASSERT_TRUE(aotFile);
    ASSERT_TRUE(strcmp(CMDLINE, aotFile->GetCommandLine()) == 0U);
    ASSERT_TRUE(strcmp(tmpfilePn, aotFile->GetFileName()) == 0U);
    ASSERT_EQ(aotFile->GetFilesCount(), 1U);

    auto pfile = aotManager->FindPandaFile(TMPFILE_PF);
    ASSERT_NE(pfile, nullptr);
    auto cls = pfile->GetClass(13U);
    ASSERT_TRUE(cls.IsValid());

    {
        auto code = cls.FindMethodCodeEntry(0U);
        ASSERT_FALSE(code == nullptr);
        ASSERT_EQ(methodName, reinterpret_cast<const char *>(code));
    }

    {
        auto code = cls.FindMethodCodeEntry(1U);
        ASSERT_FALSE(code == nullptr);
        ASSERT_EQ(std::memcmp(x86Add.data(), code, x86Add.size()), 0U);
#ifdef PANDA_TARGET_AMD64
        auto funcAdd = reinterpret_cast<int (*)(int, int)>(const_cast<void *>(code));
        ASSERT_EQ(funcAdd(2U, 3U), 5U);
#endif
    }
}

TEST_F(AotTest, PaocClusters)
{
    // Test basic functionality only in host mode.
    if (RUNTIME_ARCH != Arch::X86_64) {
        return;
    }

    TmpFile paocClusters("clusters.json");
    std::ofstream(paocClusters.GetFileName()) <<
        R"(
    {
        "clusters_map" :
        {
            "A::count" : ["unroll_enable"],
            "B::count2" : ["unroll_disable"],
            "_GLOBAL::main" : ["inline_disable", 1]
        },

        "compiler_options" :
        {
            "unroll_disable" :
            {
                "compiler-loop-unroll" : "false"
            },

            "unroll_enable" :
            {
                "compiler-loop-unroll" : "true",
                "compiler-loop-unroll-factor" : 42,
                "compiler-loop-unroll-inst-limit" : 850
            },

            "inline_disable" :
            {
                "compiler-inlining" : "false"
            }
        }
    }
    )";

    TmpFile pandaFname("test.pf");
    auto source = R"(
        .record A {}
        .record B {}

        .function i32 A.count() <static> {
            movi v1, 5
            ldai 0
        main_loop:
            jeq v1, main_ret
            addi 1
            jmp main_loop
        main_ret:
            return
        }

        .function i32 B.count() <static> {
            movi v1, 5
            ldai 0
        main_loop:
            jeq v1, main_ret
            addi 1
            jmp main_loop
        main_ret:
            return
        }

        .function i32 B.count2() <static> {
            movi v1, 5
            ldai 0
        main_loop:
            jeq v1, main_ret
            addi 1
            jmp main_loop
        main_ret:
            return
        }

        .function i32 main() {
            call.short A.count
            sta v0
            call.short B.count
            add2 v0
            call.short B.count2
            add2 v0
            return
        }
    )";

    {
        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname.GetFileName(), res.Value()));
    }

    {
        TmpFile compilerEvents("events.csv");
        auto res =
            os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname.GetFileName(), "--paoc-clusters",
                           paocClusters.GetFileName(), "--compiler-loop-unroll-factor=7",
                           "--compiler-enable-events=true", "--compiler-events-path", compilerEvents.GetFileName());
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U);

        bool firstFound = false;
        bool secondFound = false;
        std::ifstream eventsFile(compilerEvents.GetFileName());

        std::regex rgxUnrollAppliedCluster("A::count,loop-unroll,.*,unroll_factor:42,.*");
        std::regex rgxUnrollRestoredDefault("B::count,loop-unroll,.*,unroll_factor:7,.*");

        for (std::string line; std::getline(eventsFile, line);) {
            if (line.rfind("loop-unroll") != std::string::npos) {
                if (!firstFound) {
                    // Check that the cluster is applied:
                    ASSERT_TRUE(std::regex_match(line, rgxUnrollAppliedCluster));
                    firstFound = true;
                    continue;
                }
                ASSERT_FALSE(secondFound);
                // Check that the option is restored:
                ASSERT_TRUE(std::regex_match(line, rgxUnrollRestoredDefault));
                secondFound = true;
            }
        }
        ASSERT_TRUE(firstFound && secondFound);
    }
}

TEST_F(AotTest, PandaFiles)
{
#ifndef PANDA_EVENTS_ENABLED
    GTEST_SKIP();
#endif

    if (RUNTIME_ARCH != Arch::X86_64) {
        GTEST_SKIP();
    }

    TmpFile aotFname("./test.an");
    TmpFile pandaFname1("test1.pf");
    TmpFile pandaFname2("test2.pf");
    TmpFile paocOutputName("events-out.csv");

    {
        auto source = R"(
            .record Z {}
            .function i32 Z.zoo() <static> {
                ldai 45
                return
            }
        )";

        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname1.GetFileName(), res.Value()));
    }

    {
        auto source = R"(
            .record Z <external>
            .function i32 Z.zoo() <external, static>
            .record X {}
            .function i32 X.main() {
                call.short Z.zoo
                return
            }
        )";

        pandasm::Parser parser;
        auto res = parser.Parse(source);
        ASSERT_TRUE(res);
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(pandaFname2.GetFileName(), res.Value()));
    }

    {
        std::stringstream pandaFiles;
        pandaFiles << pandaFname1.GetFileName() << ',' << pandaFname2.GetFileName();
        auto res = os::exec::Exec(GetPaocFile(), "--paoc-panda-files", pandaFname2.GetFileName(), "--panda-files",
                                  pandaFname1.GetFileName(), "--events-output=csv", "--events-file",
                                  paocOutputName.GetFileName());
        ASSERT_TRUE(res) << "paoc failed with error: " << res.Error().ToString();
        ASSERT_EQ(res.Value(), 0U);

        std::ifstream infile(paocOutputName.GetFileName());
        // Inlining attempt proofs that Z::zoo was available to inline
        std::regex rgx("Inline,.*Z::zoo.*");
        bool inlineAttempt = false;
        for (std::string line; std::getline(infile, line);) {
            inlineAttempt |= std::regex_match(line, rgx);
        }
        ASSERT_TRUE(inlineAttempt);
    }
}
// NOLINTEND(readability-magic-numbers)

}  // namespace panda::compiler
