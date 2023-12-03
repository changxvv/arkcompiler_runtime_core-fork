/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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
#include <string>
#include "disassembler.h"

using namespace testing::ext;

namespace panda::disasm {
class DisasmTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

/**
* @tc.name: disassembler_column_number_test_001
* @tc.desc: Check abc file column number function.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(DisasmTest, disassembler_column_number_test_001, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "column-number1.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    // The known column number in the abc file
    std::vector<size_t> expectedColumnNumber = {10, 14, 10, 6, 2, 8, 4, 8, 4, 0};
    std::vector<size_t> columnNumber = disasm.GetColumnNumber();
    EXPECT_TRUE(expectedColumnNumber.size() == columnNumber.size());
    bool res = true;
    for (size_t i = 0; i < expectedColumnNumber.size(); ++i) {
        if (expectedColumnNumber[i] != columnNumber[i]) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
* @tc.name: disassembler_column_number_test_002
* @tc.desc: Check abc file column number function.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(DisasmTest, disassembler_column_number_test_002, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "column-number2.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    // The known column number in the abc file
    std::vector<size_t> expectedColumnNumber = {10, 6, 10, 6, 10, 6, 2, 0};
    std::vector<size_t> columnNumber = disasm.GetColumnNumber();
    EXPECT_TRUE(expectedColumnNumber.size() == columnNumber.size());
    bool res = true;
    for (size_t i = 0; i < expectedColumnNumber.size(); ++i) {
        if (expectedColumnNumber[i] != columnNumber[i]) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
* @tc.name: disassembler_column_number_test_003
* @tc.desc: Check abc file column number function.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(DisasmTest, disassembler_column_number_test_003, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "column-number3.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    // The known column number in the abc file
    std::vector<size_t> expectedColumnNumber = {4, 16, 4, 15, 4, 13, 4, 14, 6, 14, 6, 0};
    std::vector<size_t> columnNumber = disasm.GetColumnNumber();
    EXPECT_TRUE(expectedColumnNumber.size() == columnNumber.size());
    bool res = true;
    for (size_t i = 0; i < expectedColumnNumber.size(); ++i) {
        if (expectedColumnNumber[i] != columnNumber[i]) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
* @tc.name: disassembler_column_number_test_004
* @tc.desc: Check abc file column number function.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(DisasmTest, disassembler_column_number_test_004, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "column-number4.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::vector<size_t> expectedColumnNumber = {10, 14, 10, 6, 10, 6, 10, 14, 10, 6, 9, 2, 2, 4, 8, 4, 8, 4, 0};
    std::vector<size_t> columnNumber = disasm.GetColumnNumber();
    EXPECT_TRUE(expectedColumnNumber.size() == columnNumber.size());
    bool res = true;
    for (size_t i = 0; i < expectedColumnNumber.size(); ++i) {
        if (expectedColumnNumber[i] != columnNumber[i]) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}

/**
* @tc.name: disassembler_column_number_test_005
* @tc.desc: Check abc file column number function.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(DisasmTest, disassembler_column_number_test_005, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "column-number5.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    // The known column number in the abc file
    std::vector<size_t> expectedColumnNumber = {4, 16, 4, 15, 4, 13, 4, 10, 6, 10, 6, 10, 14, 10, 6,
                                                10, 14, 10, 6, 9, 2, 2, 14, 6, 14, 6, 8, 4, 0};
    std::vector<size_t> columnNumber = disasm.GetColumnNumber();
    EXPECT_TRUE(expectedColumnNumber.size() == columnNumber.size());
    bool res = true;
    for (size_t i = 0; i < expectedColumnNumber.size(); ++i) {
        if (expectedColumnNumber[i] != columnNumber[i]) {
            res = false;
            break;
        }
    }
    EXPECT_TRUE(res);
}
}
