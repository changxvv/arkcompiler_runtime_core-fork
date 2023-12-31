/**
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

#include "gtest/gtest.h"
#include "test_frame.h"
#include "debug_info_cache.h"
#include "runtime_options.h"
#include "runtime.h"
#include "types/numeric_id.h"
#include "assembly-emitter.h"
#include "assembly-parser.h"

// NOLINTBEGIN

namespace panda::tooling::inspector::test {

const char *source = R"(
    .record Test {}

    .function i32 Test.foo(u64 a0, u64 a1) {
        mov v0, v1         # line 2, offset 0,1
        mov v100, v101     # line 3, offset 2,3,4
        movi v0, 4         # line 4, offset 5,6
        ldai 222           # line 5, offset 7,8,9
        return             # line 6, offset 10
    }
)";

class DebugInfoCacheTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        pandasm::Parser p;

        auto res = p.Parse(source, "source.pa");
        ASSERT_TRUE(res.HasValue());
        ASSERT_TRUE(pandasm::AsmEmitter::Emit(file_name, res.Value()));
        auto pf = panda_file::OpenPandaFile(file_name);
        ASSERT_NE(pf, nullptr);

        cache.AddPandaFile(*pf);

        RuntimeOptions options;
        options.SetShouldInitializeIntrinsics(false);
        options.SetShouldLoadBootPandaFiles(false);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();

        thread_->ManagedCodeBegin();

        ClassLinker *class_linker = Runtime::GetCurrent()->GetClassLinker();
        class_linker->AddPandaFile(std::move(pf));
        PandaString descriptor;
        auto *ext = class_linker->GetExtension(panda_file::SourceLang::PANDA_ASSEMBLY);
        Class *klass = ext->GetClass(ClassHelper::GetDescriptor(utf::CStringAsMutf8("Test"), &descriptor));
        ASSERT_NE(klass, nullptr);
        auto methods = klass->GetMethods();
        ASSERT_EQ(methods.size(), 1);

        thread_->ManagedCodeEnd();

        method_foo = &methods[0];
    }

    static void TearDownTestSuite()
    {
        Runtime::Destroy();
    }

    static const std::string file_name;
    static DebugInfoCache cache;
    static panda::MTManagedThread *thread_;
    static Method *method_foo;
};

DebugInfoCache DebugInfoCacheTest::cache {};
panda::MTManagedThread *DebugInfoCacheTest::thread_ = nullptr;
const std::string DebugInfoCacheTest::file_name = "source.abc";
Method *DebugInfoCacheTest::method_foo = nullptr;

TEST_F(DebugInfoCacheTest, GetCurrentLineLocations)
{
    auto fr0 = TestFrame(method_foo, 2);   // offset 2, line 3 of function
    auto fr1 = TestFrame(method_foo, 6);   // offset 6, line 4 of function
    auto fr2 = TestFrame(method_foo, 10);  // offset 10, line 6 of function

    auto curr = cache.GetCurrentLineLocations(fr0);
    ASSERT_EQ(curr.size(), 3);
    ASSERT_NE(curr.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 2)), curr.end());
    ASSERT_NE(curr.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 3)), curr.end());
    ASSERT_NE(curr.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 4)), curr.end());

    curr = cache.GetCurrentLineLocations(fr1);
    ASSERT_EQ(curr.size(), 2);
    ASSERT_NE(curr.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 5)), curr.end());
    ASSERT_NE(curr.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 6)), curr.end());

    curr = cache.GetCurrentLineLocations(fr2);
    ASSERT_EQ(curr.size(), 1);
    ASSERT_NE(curr.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 10)), curr.end());
}

TEST_F(DebugInfoCacheTest, GetLocals)
{
    auto fr0 = TestFrame(method_foo, 2);  // offset 2, line 3 of function

    fr0.SetArgument(0, 1);
    fr0.SetArgument(1, 2);
    fr0.SetArgumentKind(0, PtFrame::RegisterKind::PRIMITIVE);
    fr0.SetArgumentKind(1, PtFrame::RegisterKind::PRIMITIVE);

    for (size_t i = 0; i < 103; i++) {
        fr0.SetVReg(i, i);
        fr0.SetVRegKind(i, PtFrame::RegisterKind::PRIMITIVE);
    }
    auto map_locals = cache.GetLocals(fr0);
    ASSERT_EQ(105, map_locals.size());

    EXPECT_NO_THROW(map_locals.at("a0"));
    EXPECT_NO_THROW(map_locals.at("a1"));
    EXPECT_NO_THROW(map_locals.at("v101"));

    ASSERT_EQ(map_locals.at("a0").GetAsU64(), 1);
    ASSERT_EQ(map_locals.at("a1").GetAsU64(), 2);
    ASSERT_EQ(map_locals.at("v101").GetAsU64(), 101);
}

TEST_F(DebugInfoCacheTest, GetSourceLocation)
{
    auto fr0 = TestFrame(method_foo, 2);  // offset 2, line 3 of function
    auto fr1 = TestFrame(method_foo, 6);  // offset 6, line 4 of function

    std::string_view disasm_file;
    std::string_view method_name;
    size_t line_number = 0;

    cache.GetSourceLocation(fr0, disasm_file, method_name, line_number);
    ASSERT_NE(disasm_file.find(file_name), std::string::npos);
    ASSERT_EQ(method_name, "foo");
    ASSERT_EQ(line_number, 3);

    cache.GetSourceLocation(fr1, disasm_file, method_name, line_number);
    ASSERT_NE(disasm_file.find(file_name), std::string::npos);
    ASSERT_EQ(method_name, "foo");
    ASSERT_EQ(line_number, 4);

    auto set_locs = cache.GetContinueToLocations(disasm_file, 4);
    ASSERT_EQ(set_locs.size(), 2);
    ASSERT_NE(set_locs.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 6)), set_locs.end());
    ASSERT_NE(set_locs.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 5)), set_locs.end());

    set_locs = cache.GetContinueToLocations(disasm_file, 6);
    ASSERT_EQ(set_locs.size(), 1);
    ASSERT_NE(set_locs.find(PtLocation(file_name.c_str(), method_foo->GetFileId(), 10)), set_locs.end());

    set_locs = cache.GetContinueToLocations(disasm_file, 1);
    ASSERT_EQ(set_locs.size(), 0);

    auto valid_locs = cache.GetValidLineNumbers(disasm_file, 0, 100, false);
    ASSERT_EQ(valid_locs.size(), 5);

    ASSERT_NE(valid_locs.find(2), valid_locs.end());
    ASSERT_NE(valid_locs.find(3), valid_locs.end());
    ASSERT_NE(valid_locs.find(4), valid_locs.end());
    ASSERT_NE(valid_locs.find(5), valid_locs.end());
    ASSERT_NE(valid_locs.find(6), valid_locs.end());

    auto s = cache.GetSourceCode(disasm_file);
    ASSERT_NE(s.find(".function i32 Test.foo(u64 a0, u64 a1)"), std::string::npos);

    s = cache.GetSourceCode("source.pa");
    ASSERT_TRUE(s.empty());

    std::set<std::string_view> sets;
    auto breaks = cache.GetBreakpointLocations([](auto) { return true; }, 4, sets);
    ASSERT_EQ(breaks.size(), 1);
    ASSERT_EQ(sets.size(), 1);
    ASSERT_EQ(*sets.begin(), disasm_file);

    ASSERT_NE(std::find(breaks.begin(), breaks.end(), PtLocation(file_name.c_str(), method_foo->GetFileId(), 5)),
              breaks.end());
}

}  // namespace panda::tooling::inspector::test

// NOLINTEND