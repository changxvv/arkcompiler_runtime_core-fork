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

#include <compiler/compiler_options.h>
#include <gtest/gtest.h>

#include <bitset>
#include <vector>

#include "assembly-emitter.h"
#include "assembly-parser.h"
#include "code_info/code_info_builder.h"
#include "libpandabase/utils/cframe_layout.h"
#include "runtime/bridge/bridge.h"
#include "runtime/include/runtime.h"
#include "runtime/include/stack_walker-inl.h"
#include "runtime/include/thread_scopes.h"
#include "runtime/mem/refstorage/global_object_storage.h"
#include "runtime/tests/test_utils.h"
#include "compiler/tests/panda_runner.h"

namespace panda::test {

// NOLINTNEXTLINE(google-build-using-namespace)
using namespace compiler;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define HOOK_ASSERT(cond, action)                                                     \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::cerr << "ASSERT FAILED(" << __LINE__ << "): " << #cond << std::endl; \
            action;                                                                   \
        }                                                                             \
    } while (0)

class StackWalkerTest : public testing::Test {
public:
    using Callback = int (*)(uintptr_t, uintptr_t);

    StackWalkerTest()
        : default_compiler_non_optimizing_(OPTIONS.IsCompilerNonOptimizing()),
          default_compiler_regex_(OPTIONS.GetCompilerRegex())
    {
    }

    ~StackWalkerTest() override
    {
        OPTIONS.SetCompilerNonOptimizing(default_compiler_non_optimizing_);
        OPTIONS.SetCompilerRegex(default_compiler_regex_);
    }

    NO_COPY_SEMANTIC(StackWalkerTest);
    NO_MOVE_SEMANTIC(StackWalkerTest);

    void SetUp() override
    {
#ifndef PANDA_COMPILER_ENABLE
        GTEST_SKIP();
#endif
    }

    void TestModifyManyVregs(bool is_compiled);

    static Method *GetMethod(std::string_view method_name)
    {
        PandaString descriptor;
        auto *thread = MTManagedThread::GetCurrent();
        thread->ManagedCodeBegin();
        auto *extension = Runtime::GetCurrent()->GetClassLinker()->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        auto cls = extension->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("_GLOBAL"), &descriptor));
        thread->ManagedCodeEnd();
        ASSERT(cls);
        return cls->GetDirectMethod(utf::CStringAsMutf8(method_name.data()));
    }

    static mem::Reference *global_obj_;

private:
    bool default_compiler_non_optimizing_;
    std::string default_compiler_regex_;
};

mem::Reference *StackWalkerTest::global_obj_;

template <typename T>
uint64_t ConvertToU64(T val)
{
    if constexpr (sizeof(T) == sizeof(uint32_t)) {
        return bit_cast<uint32_t>(val);
    } else if constexpr (sizeof(T) == sizeof(uint64_t)) {
        return bit_cast<uint64_t>(val);
    } else {
        return static_cast<uint64_t>(val);
    }
}

template <Arch ARCH>
int32_t ToCalleeRegister(size_t reg)
{
    return reg + GetFirstCalleeReg(ARCH, false);
}

template <Arch ARCH>
int32_t ToCalleeFpRegister(size_t reg)
{
    return reg + GetFirstCalleeReg(ARCH, true);
}

TEST_F(StackWalkerTest, ModifyVreg)
{
    auto source = R"(
        .function i32 main() {
            movi.64 v0, 27
        try_begin:
            call.short testa, v0
            jmp try_end
        try_end:
            return
        .catchall try_begin, try_end, try_end
        }
        .function i32 testa(i64 a0) {
            call.short testb, a0
            return
        }
        .function i32 testb(i64 a0) {
            call.short testc, a0
            return
        }
        .function i32 testc(i64 a0) {
            lda a0
        try_begin:
            call.short hook  # change vregs in all frames
            jnez exit
            call.short hook  # verify vregs in all frames
            jmp exit
        exit:
            return
        .catchall try_begin, exit, exit
        }
        .function i32 hook() {
            ldai 1
            return
        }
    )";

    PandaRunner runner;
    runner.GetRuntimeOptions().SetCompilerHotnessThreshold(0);
    runner.GetCompilerOptions().SetCompilerNonOptimizing(true);
    runner.GetCompilerOptions().SetCompilerRematConst(false);
    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::testb|_GLOBAL::hook).*");
    [[maybe_unused]] static constexpr std::array<uint64_t, 3> FRAME_VALUES = {0x123456789abcdef, 0xaaaabbbbccccdddd,
                                                                              0xabcdef20};
    static int run_count = 0;
    runner.Run(source, [](uintptr_t lr, [[maybe_unused]] uintptr_t fp) -> int {
        StackWalker walker(reinterpret_cast<void *>(fp), true, lr);
        bool success = false;
        walker.NextFrame();
        if (run_count == 0) {
            bool was_set = false;
            HOOK_ASSERT(!walker.IsCFrame(), return 1);
            success = walker.IterateVRegsWithInfo([&was_set, &walker](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    HOOK_ASSERT(reg.GetLong() == 27, return false);
                    walker.SetVRegValue(reg_info, FRAME_VALUES[0]);
                    was_set = true;
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);
            HOOK_ASSERT(was_set, return 1);

            walker.NextFrame();
            HOOK_ASSERT(walker.IsCFrame(), return 1);
            success = walker.IterateVRegsWithInfo([&walker](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    HOOK_ASSERT(reg.GetLong() == 27, return false);
                    walker.SetVRegValue(reg_info, FRAME_VALUES[1]);
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);

            walker.NextFrame();
            HOOK_ASSERT(walker.IsCFrame(), return 1);
            success = walker.IterateVRegsWithInfo([&walker](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    HOOK_ASSERT(reg.GetLong() == 27, return true;);
                    walker.SetVRegValue(reg_info, FRAME_VALUES[2]);
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);
        } else if (run_count == 1) {
            HOOK_ASSERT(!walker.IsCFrame(), return 1);
            success = walker.IterateVRegsWithInfo([](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    HOOK_ASSERT(reg.GetLong() == bit_cast<int64_t>(FRAME_VALUES[0]), return true;);
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);

            walker.NextFrame();
            HOOK_ASSERT(walker.IsCFrame(), return 1);
            success = walker.IterateVRegsWithInfo([](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    HOOK_ASSERT(reg.GetLong() == bit_cast<int64_t>(FRAME_VALUES[1]), return true;);
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);

            walker.NextFrame();
            HOOK_ASSERT(walker.IsCFrame(), return 1);
            success = walker.IterateVRegsWithInfo([](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    HOOK_ASSERT(reg.GetLong() == bit_cast<int64_t>(FRAME_VALUES[2]), return true;);
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);
        } else {
            return 1;
        }
        run_count++;
        return 0;
    });
    ASSERT_EQ(run_count, 2);
}

void StackWalkerTest::TestModifyManyVregs(bool is_compiled)
{
    auto source = R"(
        .function i32 main() {
            movi.64 v0, 5
        try_begin:
            call.short test
            jmp try_end
        try_end:
            return

        .catchall try_begin, try_end, try_end
        }

        .function i32 test() {
            movi.64 v1, 1
            movi.64 v2, 2
            movi.64 v3, 3
            movi.64 v4, 4
            movi.64 v5, 5
            movi.64 v6, 6
            movi.64 v7, 7
            movi.64 v8, 8
            movi.64 v9, 9
            movi.64 v10, 10
            movi.64 v11, 11
            movi.64 v12, 12
            movi.64 v13, 13
            movi.64 v14, 14
            movi.64 v15, 15
            movi.64 v16, 16
            movi.64 v17, 17
            movi.64 v18, 18
            movi.64 v19, 19
            movi.64 v20, 20
            movi.64 v21, 21
            movi.64 v22, 22
            movi.64 v23, 23
            movi.64 v24, 24
            movi.64 v25, 25
            movi.64 v26, 26
            movi.64 v27, 27
            movi.64 v28, 28
            movi.64 v29, 29
            movi.64 v30, 30
            movi.64 v31, 31
        try_begin:
            mov v0, v31
            newarr v0, v0, i32[]
            call.short stub
            jmp try_end
        try_end:
            return

        .catchall try_begin, try_end, try_end
        }

        .function i32 stub() {
        try_begin:
            call.short hook  # change vregs in all frames
            jnez exit
            call.short hook  # verify vregs in all frames
            jmp exit
        exit:
            return

        .catchall try_begin, exit, exit
        }

        .function i32 hook() {
            ldai 1
            return
        }
    )";

    static bool first_run;
    static bool compiled;

    PandaRunner runner;
    runner.GetRuntimeOptions().SetCompilerHotnessThreshold(0);
    runner.GetCompilerOptions().SetCompilerNonOptimizing(true);

    if (!is_compiled) {
        runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main)(?!_GLOBAL::test)(?!_GLOBAL::hook).*");
    }

    first_run = true;
    compiled = is_compiled;
    runner.Run(source, [](uintptr_t lr, [[maybe_unused]] uintptr_t fp) -> int {
        StackWalker walker(reinterpret_cast<void *>(fp), true, lr);

        HOOK_ASSERT(walker.GetMethod()->GetFullName() == "_GLOBAL::stub", return 1);
        walker.NextFrame();
        HOOK_ASSERT(walker.GetMethod()->GetFullName() == "_GLOBAL::test", return 1);
        HOOK_ASSERT(walker.IsCFrame() == compiled, return 1);

        int reg_index = 1;
        bool success = false;
        if (first_run) {
            auto storage = Runtime::GetCurrent()->GetPandaVM()->GetGlobalObjectStorage();
            StackWalkerTest::global_obj_ =
                storage->Add(panda::mem::AllocateNullifiedPayloadString(1), mem::Reference::ObjectType::GLOBAL);
        }
        auto obj = Runtime::GetCurrent()->GetPandaVM()->GetGlobalObjectStorage()->Get(StackWalkerTest::global_obj_);
        if (first_run) {
            success = walker.IterateVRegsWithInfo([&reg_index, &walker, &obj](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    if (reg.HasObject()) {
                        HOOK_ASSERT(reg.GetReference() != nullptr, return false);
                        walker.SetVRegValue(reg_info, obj);
                    } else if (reg_info.GetLocation() != VRegInfo::Location::CONSTANT) {
                        // frame_size is more than nregs_ now for inst `V4_V4_ID16` and `V4_V4_V4_V4_ID16`
                        auto reg_idx = reg_info.GetIndex();
                        if (reg_idx < walker.GetMethod()->GetNumVregs()) {
                            HOOK_ASSERT(reg_idx == reg.GetLong(), return false);
                        }
                        // NOLINTNEXTLINE(readability-magic-numbers)
                        walker.SetVRegValue(reg_info, reg_idx + 100000000000);
                    }
                    reg_index++;
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);
            HOOK_ASSERT(reg_index >= 32, return 1);
            first_run = false;
        } else {
            success = walker.IterateVRegsWithInfo([&reg_index, &obj](const auto &reg_info, const auto &reg) {
                if (!reg_info.IsAccumulator()) {
                    if (reg.HasObject()) {
                        HOOK_ASSERT((reg.GetReference() == reinterpret_cast<ObjectHeader *>(Low32Bits(obj))),
                                    return false);
                    } else {
                        if (reg_info.GetLocation() != VRegInfo::Location::CONSTANT) {
                            HOOK_ASSERT(reg.GetLong() == (reg_info.GetIndex() + 100000000000), return false);
                        }
                        reg_index++;
                    }
                }
                return true;
            });
            HOOK_ASSERT(success, return 1);
            HOOK_ASSERT(reg_index >= 32, return 1);
        }

        HOOK_ASSERT(success, return 1);
        return 0;
    });
}

TEST_F(StackWalkerTest, ModifyMultipleVregs)
{
    if constexpr (ArchTraits<RUNTIME_ARCH>::SUPPORT_DEOPTIMIZATION) {
        TestModifyManyVregs(true);
        TestModifyManyVregs(false);
    }
}

TEST_F(StackWalkerTest, ThrowExceptionThroughMultipleFrames)
{
    auto source = R"(
        .record E {}

        .function u1 f4() {
            newobj v0, E
            throw v0
            return
        }

        .function u1 f3() {
            call f4
            return
        }

        .function u1 f2() {
            call f3
            return
        }

        .function u1 f1() {
            call f2
            return
        }

        .function u1 main() {
        try_begin:
            movi v0, 123
            call f1
            ldai 1
            return
        try_end:

        catch_block1_begin:
            movi v1, 123
            lda v0
            jne v1, exit_1
            ldai 0
            return
            exit_1:
            ldai 1
            return

        .catch E, try_begin, try_end, catch_block1_begin
        }
    )";

    PandaRunner runner;
    runner.GetRuntimeOptions().SetCompilerHotnessThreshold(0);
    runner.GetCompilerOptions().SetCompilerNonOptimizing(true);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f4|_GLOBAL::f1|_GLOBAL::f2|_GLOBAL::f3).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f4|_GLOBAL::f1|_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f4|_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f4).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f1|_GLOBAL::f2|_GLOBAL::f3).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f4|_GLOBAL::f1|_GLOBAL::f2|_GLOBAL::f3).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f4|_GLOBAL::f1|_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f4|_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f4).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex(".*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f1|_GLOBAL::f2|_GLOBAL::f3).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f1|_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex(".*");
    runner.Run(source, nullptr);
}

TEST_F(StackWalkerTest, CatchInCompiledCode)
{
    auto source = R"(
        .record panda.String <external>
        .record panda.ArrayIndexOutOfBoundsException <external>

        .function i32 f2() {
            movi v0, 4
            newarr v1, v0, i64[]
            ldai 42
            ldarr v1 # BoundsException
            return
        }

        .function i32 f1() {
        try_begin:
            call f2
            ldai 1
            return
        try_end:

        catch_block1_begin:
            ldai 123
            return

        .catch panda.ArrayIndexOutOfBoundsException, try_begin, try_end, catch_block1_begin
        }

        .function u1 main() {
            call f1
            movi v0, 123
            jne v0, exit_1
            ldai 0
            return
            exit_1:
            ldai 1
            return
        }
    )";

    PandaRunner runner;
    runner.GetRuntimeOptions().SetCompilerHotnessThreshold(0);
    runner.GetCompilerOptions().SetCompilerNonOptimizing(true);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f2).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main|_GLOBAL::f1).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex("(?!_GLOBAL::main).*");
    runner.Run(source, nullptr);

    runner.GetCompilerOptions().SetCompilerRegex(".*");
    runner.Run(source, nullptr);
}

}  // namespace panda::test
