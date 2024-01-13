/*
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

#include <iostream>
#include <string>
#include <regex>

#include <gtest/gtest.h>
#include "disassembler.h"
#include "assembly-parser.h"

static inline std::string ExtractFuncBody(const std::string &text, const std::string &header)
{
    auto beg = text.find(header);
    auto end = text.find('}', beg);

    ASSERT(beg != std::string::npos);
    ASSERT(end != std::string::npos);

    return text.substr(beg + header.length(), end - (beg + header.length()));
}

TEST(MetadataTest, test1)
{
    auto program = panda::pandasm::Parser().Parse(R"(
.record B <external>

.record A {
}

.function u1 A.BBB(u1 a0) <ctor> {}
.function u1 A.CCC(u1 a0) <cctor> {}

.function u1 DDD(u1 a0) <external>

.function u1 A.EEE(A a0, u1 a1) {
    call DDD, v0
    initobj A.BBB, v0, v1
    initobj A.CCC, v1, v2
}

.function u1 FFF() <noimpl>

.function u1 GGG() <native>
    )");
    ASSERT(program);
    auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
    ASSERT(pf);

    panda::disasm::Disassembler d {};
    std::stringstream ss {};

    d.Disassemble(pf);
    d.Serialize(ss);

    std::string prog = ss.str();

    EXPECT_TRUE(prog.find(".function u1 GGG() <native, static>") != std::string::npos);
    EXPECT_TRUE(prog.find(".function u1 FFF() <noimpl, static>") != std::string::npos);
    EXPECT_TRUE(prog.find(".function u1 A._cctor_(u1 a0) <cctor, static> {\n}") != std::string::npos);
    EXPECT_TRUE(prog.find(".function u1 A._ctor_(u1 a0) <ctor, static> {\n}") != std::string::npos);
    EXPECT_TRUE(prog.find(".function u1 DDD(u1 a0) <external, static>") != std::string::npos);
    EXPECT_TRUE(prog.find(".record A {\n}") != std::string::npos);
    EXPECT_TRUE(prog.find(".record B <external>") != std::string::npos);

    std::string bodyAEee = ExtractFuncBody(ss.str(), ".function u1 A.EEE(A a0, u1 a1) {\n");
    std::stringstream aEee {bodyAEee};

    std::string line;
    std::getline(aEee, line);
    EXPECT_EQ("\tcall.short DDD:(u1), v0", line);
    std::getline(aEee, line);
    EXPECT_EQ("\tinitobj.short A._ctor_:(u1), v0", line);
    std::getline(aEee, line);
    EXPECT_EQ("\tinitobj.short A._cctor_:(u1), v1", line);
}

TEST(MetadataTest, ExternalFieldTest)
{
    auto program = panda::pandasm::Parser().Parse(R"(
.record B <external> {
    i32 fieldB <external>
}

.function void main() <static> {
    ldstatic.obj B.fieldB
    return.void
}
    )");
    ASSERT(program);
    auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
    ASSERT(pf);

    panda::disasm::Disassembler d {};
    std::stringstream ss {};

    d.Disassemble(pf);
    d.Serialize(ss);

    std::string prog = ss.str();

    EXPECT_TRUE(prog.find(".record B <external> {") != std::string::npos);
    EXPECT_TRUE(prog.find("\ti32 fieldB <external>") != std::string::npos);

    std::string bodyAEee = ExtractFuncBody(ss.str(), ".function void main() <static> {\n");
    std::stringstream aEee {bodyAEee};

    std::string line;
    std::getline(aEee, line);
    EXPECT_EQ("\tldstatic.obj B.fieldB", line);
    std::getline(aEee, line);
    EXPECT_EQ("\treturn.void", line);
}

TEST(MetadataTest, Access)
{
    auto matchString = [](const char *pattern, const std::string &str) {
        auto regex = std::regex(pattern);
        auto m = std::smatch {};
        std::regex_search(str, m, regex);

        return m.size() == 1;
    };

    {
        auto program = panda::pandasm::Parser().Parse(R"(
        .record A <access.record=public> {
            i64 a
        }

        .record B <access.record=protected> { }

        .record C <access.record=private> {
            B b
        }
    )");
        ASSERT(program);
        auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
        ASSERT(pf);

        panda::disasm::Disassembler d {};
        std::stringstream ss {};

        d.Disassemble(pf);
        d.Serialize(ss);

        EXPECT_TRUE(matchString("[.record A][^.]*[.record=public]", ss.str()));
        EXPECT_TRUE(matchString("[.record B][^.]*[.record=protected]", ss.str()));
        EXPECT_TRUE(matchString("[.record C][^.]*[.record=private]", ss.str()));
    }
    {
        auto program = panda::pandasm::Parser().Parse(R"(
        .record A <access.record=protected> {
            i32 pub <access.field=public>
            i32 prt <access.field=protected>
            i32 prv <access.field=private>
        }

        .function void f() <access.function=public> {}
        .function void A.g() <access.function=protected> {}
        .function void h() <access.function=private> {}
    )");
        ASSERT(program);
        auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
        ASSERT(pf);

        panda::disasm::Disassembler d {};
        std::stringstream ss {};

        d.Disassemble(pf);
        d.Serialize(ss);

        auto str = ss.str();
        EXPECT_TRUE(matchString("[.record A][^.]*[.record=protected]>", str));
        EXPECT_TRUE(matchString("[i32 pub][^.]*[.field=public]>", str));
        EXPECT_TRUE(matchString("[i32 prt][^.]*[.field=protected]>", str));
        EXPECT_TRUE(matchString("[i32 prv][^.]*[.field=private]>", str));
        EXPECT_TRUE(matchString("[void f()][^.]*[.function=public]>", str));
        EXPECT_TRUE(matchString("[void A.g()][^.]*[.function=protected]>", str));
        EXPECT_TRUE(matchString("[void h()][^.]*[.function=private]>", str));
    }
}

TEST(MetadataTest, Final)
{
    auto matchString = [](const char *pattern, const std::string &str) {
        auto regex = std::regex(pattern);
        auto m = std::smatch {};
        std::regex_search(str, m, regex);

        return m.size() == 1;
    };

    {
        auto program = panda::pandasm::Parser().Parse(R"(
            .record A <final> {
                i64 a
            }
        )");
        ASSERT(program);
        auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
        ASSERT(pf);

        panda::disasm::Disassembler d {};
        std::stringstream ss {};

        d.Disassemble(pf);
        d.Serialize(ss);

        EXPECT_TRUE(matchString("[.record A][^.]*[final]>", ss.str()));
    }
    {
        auto program = panda::pandasm::Parser().Parse(R"(
            .record A {
                i32 fld <final>
            }
        )");
        ASSERT(program);
        auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
        ASSERT(pf);

        panda::disasm::Disassembler d {};
        std::stringstream ss {};

        d.Disassemble(pf);
        d.Serialize(ss);

        auto str = ss.str();
        EXPECT_TRUE(matchString("[i32 fld][^<]*[final]>", str));
    }
    {
        auto program = panda::pandasm::Parser().Parse(R"(
            .record A { }
            .function void A.f(A a0) <final> {}
        )");
        ASSERT(program);
        auto pf = panda::pandasm::AsmEmitter::Emit(program.Value());
        ASSERT(pf);

        panda::disasm::Disassembler d {};
        std::stringstream ss {};

        d.Disassemble(pf);
        d.Serialize(ss);

        auto str = ss.str();
        std::cout << str << std::endl;
        EXPECT_TRUE(matchString("[void A.f][^<]*[final]>", str));
    }
}

#undef DISASM_BIN_DIR
