/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "runtime/include/runtime.h"
#include "runtime/regexp/ecmascript/regexp_parser.h"
#include "runtime/regexp/ecmascript/regexp_executor.h"

namespace panda::test {

// NOLINTBEGIN(readability-magic-numbers)

class RegExpTest : public testing::Test {
public:
    RegExpTest()
    {
        RuntimeOptions options;
        options.SetShouldLoadBootPandaFiles(false);
        options.SetShouldInitializeIntrinsics(false);
        Runtime::Create(options);
        thread_ = panda::MTManagedThread::GetCurrent();
        thread_->ManagedCodeBegin();
    }

    ~RegExpTest() override
    {
        thread_->ManagedCodeEnd();
        Runtime::Destroy();
    }

    NO_COPY_SEMANTIC(RegExpTest);
    NO_MOVE_SEMANTIC(RegExpTest);

    bool IsValidAlphaEscapeInAtom(char s) const
    {
        switch (s) {
            // Assertion [U] :: \b
            case 'b':
            // Assertion [U] :: \B
            case 'B':
            // ControlEscape :: one of f n r t v
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            // CharacterClassEscape :: one of d D s S w W
            case 'd':
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                return true;
            default:
                return false;
        }
    }

    bool IsValidAlphaEscapeInClass(char s) const
    {
        switch (s) {
            // ClassEscape[U] :: b
            case 'b':
            // ControlEscape :: one of f n r t v
            case 'f':
            case 'n':
            case 'r':
            case 't':
            case 'v':
            // CharacterClassEscape :: one of d D s S w W
            case 'd':
            case 'D':
            case 's':
            case 'S':
            case 'w':
            case 'W':
                return true;
            default:
                return false;
        }
    }

protected:
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    panda::MTManagedThread *thread_;
};

TEST_F(RegExpTest, ParseError1)
{
    RegExpParser parser = RegExpParser();
    PandaString source("0{2,1}");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError2)
{
    RegExpParser parser = RegExpParser();
    PandaString source("^[z-a]$");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError3)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError4)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a**");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError5)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a***");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError6)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a**");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError7)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a++");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError8)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a+++");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError9)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a???");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError10)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a????");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError11)
{
    RegExpParser parser = RegExpParser();
    PandaString source("*a");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError12)
{
    RegExpParser parser = RegExpParser();
    PandaString source("**a");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError13)
{
    RegExpParser parser = RegExpParser();
    PandaString source("+a");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError14)
{
    RegExpParser parser = RegExpParser();
    PandaString source("++a");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError15)
{
    RegExpParser parser = RegExpParser();
    PandaString source("?a");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError16)
{
    RegExpParser parser = RegExpParser();
    PandaString source("??a");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError17)
{
    RegExpParser parser = RegExpParser();
    PandaString source("x{1}{1,}");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError18)
{
    RegExpParser parser = RegExpParser();
    PandaString source("x{1,2}{1}");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError19)
{
    RegExpParser parser = RegExpParser();
    PandaString source("x{1,}{1}");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError20)
{
    RegExpParser parser = RegExpParser();
    PandaString source("x{0,1}{1,}");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError21)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[b-ac-e]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError22)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\10b-G]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError23)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\0b-G]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 0);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError24)
{
    RegExpParser parser = RegExpParser();
    PandaString source("(");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError25)
{
    RegExpParser parser = RegExpParser();
    PandaString source(")");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError26)
{
    RegExpParser parser = RegExpParser();
    PandaString source("{");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError27)
{
    RegExpParser parser = RegExpParser();
    PandaString source("}");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError28)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError29)
{
    RegExpParser parser = RegExpParser();
    PandaString source("]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError30)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\c");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError31)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\c\024");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError32)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\c]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError33)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\c\024]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError34)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\d-a]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError35)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\s-a]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError36)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\s-\\w]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError37)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[a-\\w]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError38)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\{");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_FALSE(parse_result);
}

TEST_F(RegExpTest, ParseError39)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\/");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_FALSE(parse_result);
}

TEST_F(RegExpTest, ParseError40)
{
    for (char cu = 0x41; cu <= 0x5a; ++cu) {
        if (!IsValidAlphaEscapeInAtom(cu)) {
            PandaString source("\\");
            source += PandaString(&cu, 1);
            RegExpParser parser = RegExpParser();
            parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
            parser.Parse();
            bool parse_result = parser.IsError();
            ASSERT_TRUE(parse_result);
        }
    }
    for (char cu = 0x61; cu <= 0x7a; ++cu) {
        if (!IsValidAlphaEscapeInAtom(cu)) {
            PandaString source("\\");
            source += PandaString(&cu, 1);
            RegExpParser parser = RegExpParser();
            parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
            parser.Parse();
            bool parse_result = parser.IsError();
            ASSERT_TRUE(parse_result);
        }
    }
    for (char cu = 0x41; cu <= 0x5a; ++cu) {
        PandaString source("[\\");
        if (!IsValidAlphaEscapeInAtom(cu)) {
            source += PandaString(&cu, 1);
            source += PandaString("]");
            RegExpParser parser = RegExpParser();
            parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
            parser.Parse();
            bool parse_result = parser.IsError();
            ASSERT_TRUE(parse_result);
        }
    }
    for (char cu = 0x61; cu <= 0x7a; ++cu) {
        PandaString source("[\\");
        if (!IsValidAlphaEscapeInAtom(cu)) {
            source += PandaString(&cu, 1);
            source += PandaString("]");
            RegExpParser parser = RegExpParser();
            parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
            parser.Parse();
            bool parse_result = parser.IsError();
            ASSERT_TRUE(parse_result);
        }
    }
}

TEST_F(RegExpTest, ParseError44)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\1");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError45)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\1]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError46)
{
    RegExpParser parser = RegExpParser();
    PandaString source("\\00");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseError47)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\00]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_TRUE(parse_result);
}

TEST_F(RegExpTest, ParseNoError1)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a{10,2147483648}");  // 2^31
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_FALSE(parse_result);
}

TEST_F(RegExpTest, ParseNoError2)
{
    RegExpParser parser = RegExpParser();
    PandaString source("a{10,4294967306}");  // 2^32+10
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 16);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_FALSE(parse_result);
}

TEST_F(RegExpTest, ParseNoError3)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\⥚]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 1);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_FALSE(parse_result);
}

TEST_F(RegExpTest, ParseNoError4)
{
    RegExpParser parser = RegExpParser();
    PandaString source("[\\⊲|\\⇐]");
    parser.Init(const_cast<char *>(reinterpret_cast<const char *>(source.c_str())), source.size(), 1);
    parser.Parse();
    bool parse_result = parser.IsError();
    ASSERT_FALSE(parse_result);
}

TEST_F(RegExpTest, RangeSet1)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(1, 1),
        std::make_pair(2, 2),
        std::make_pair(3, 3),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(1, 5),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Insert(4, 5);
    range_result.Compress();
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet2)
{
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(4, 5),
    };
    RangeSet range_result;
    RangeSet range_expected(list_expected);
    range_result.Insert(4, 5);
    range_result.Compress();
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet3)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(2, 2),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(1, 5),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Insert(1, 5);
    range_result.Compress();
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet4)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(1, 5),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(1, 5),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Insert(2, 4);
    range_result.Compress();
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet5)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(1, 2),
        std::make_pair(9, UINT16_MAX),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(1, 2),
        std::make_pair(4, 7),
        std::make_pair(9, UINT16_MAX),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Insert(4, 7);
    range_result.Compress();
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet6)
{
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(0, UINT16_MAX),
    };
    RangeSet range_result;
    RangeSet range_expected(list_expected);
    range_result.Invert(false);
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet7)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(1, 5),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(0, 0),
        std::make_pair(6, UINT16_MAX),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Invert(false);
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet8)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(1, 5),
        std::make_pair(0xfffe, UINT16_MAX),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(0, 0),
        std::make_pair(6, 0xfffd),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Invert(false);
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet9)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(0, 5),
        std::make_pair(0xfffe, 0xfffe),
    };
    std::list<std::pair<uint32_t, uint32_t>> list_expected = {
        std::make_pair(6, 0xfffd),
        std::make_pair(UINT16_MAX, UINT16_MAX),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected(list_expected);
    range_result.Invert(false);
    EXPECT_EQ(range_result, range_expected);
}

TEST_F(RegExpTest, RangeSet10)
{
    std::list<std::pair<uint32_t, uint32_t>> list_input = {
        std::make_pair(0, UINT16_MAX),
    };
    RangeSet range_result(list_input);
    RangeSet range_expected;
    range_result.Invert(false);
    EXPECT_EQ(range_result, range_expected);
}
}  // namespace panda::test

// NOLINTEND(readability-magic-numbers)
