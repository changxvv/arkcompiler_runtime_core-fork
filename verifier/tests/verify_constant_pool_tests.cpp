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

#include "verifier.h"

#include <cstdlib>
#include <gtest/gtest.h>
#include <string>

#include "file.h"
#include "utils.h"

using namespace testing::ext;

namespace panda::verifier {
class VerifierConstantPool : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

/**
* @tc.name: verifier_constant_pool_001
* @tc.desc: Verify abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_001, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    panda::verifier::Verifier ver {file_name};

    EXPECT_TRUE(ver.VerifyConstantPool());
}

/**
* @tc.name: verifier_constant_pool_002
* @tc.desc: Verify the method id of the abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_002, TestSize.Level1)
{
    const std::string base_file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    {
        panda::verifier::Verifier ver {base_file_name};
        EXPECT_TRUE(ver.VerifyConstantPool());
    }
    std::ifstream base_file(base_file_name, std::ios::binary);
    EXPECT_TRUE(base_file.is_open());

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(base_file), {});

    std::vector<uint8_t> new_method_id = {0xff, 0xff};
    std::vector<uint8_t> method_id = {0x0e, 0x00}; // The known method id in the abc file

    for (size_t i = buffer.size() - 1; i >= 0; --i) {
        if (buffer[i] == method_id[0] && buffer[i + 1] == method_id[1]) {
            buffer[i] = static_cast<unsigned char>(new_method_id[0]);
            buffer[i + 1] = static_cast<unsigned char>(new_method_id[1]);
            break;
        }
    }

    const std::string target_file_name = GRAPH_TEST_ABC_DIR "verifier_constant_pool_002.abc";
    GenerateModifiedAbc(buffer, target_file_name);
    base_file.close();

    {
        panda::verifier::Verifier ver {target_file_name};
        EXPECT_FALSE(ver.VerifyConstantPool());
    }
}

/**
* @tc.name: verifier_constant_pool_003
* @tc.desc: Verify the literal id of the abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_003, TestSize.Level1)
{
    const std::string base_file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    {
        panda::verifier::Verifier ver {base_file_name};
        EXPECT_TRUE(ver.VerifyConstantPool());
    }
    std::ifstream base_file(base_file_name, std::ios::binary);
    EXPECT_TRUE(base_file.is_open());

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(base_file), {});

    std::vector<uint8_t> new_literal_id = {0xac, 0xfc};
    std::vector<uint8_t> literal_id = {0x0f, 0x00}; // The known literal id in the abc file

    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == literal_id[0] && buffer[i + 1] == literal_id[1]) {
            buffer[i] = static_cast<unsigned char>(new_literal_id[0]);
            buffer[i + 1] = static_cast<unsigned char>(new_literal_id[1]);
            break;
        }
    }

    const std::string target_file_name = GRAPH_TEST_ABC_DIR "verifier_constant_pool_003.abc";

    GenerateModifiedAbc(buffer, target_file_name);
    
    base_file.close();

    {
        panda::verifier::Verifier ver {target_file_name};
        EXPECT_FALSE(ver.VerifyConstantPool());
    }
}

/**
* @tc.name: verifier_constant_pool_004
* @tc.desc: Verify the string id of the abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_004, TestSize.Level1)
{
    const std::string base_file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    {
        panda::verifier::Verifier ver {base_file_name};
        EXPECT_TRUE(ver.VerifyConstantPool());
    }
    std::ifstream base_file(base_file_name, std::ios::binary);
    EXPECT_TRUE(base_file.is_open());

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(base_file), {});

    std::vector<uint8_t> new_string_id = {0xff, 0x00};
    std::vector<uint8_t> string_id = {0x0c, 0x00}; // The known string id in the abc file

    for (size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == string_id[0] && buffer[i + 1] == string_id[1]) {
            buffer[i] = static_cast<unsigned char>(new_string_id[0]);
            buffer[i + 1] = static_cast<unsigned char>(new_string_id[1]);
            break;
        }
    }

    const std::string target_file_name = GRAPH_TEST_ABC_DIR "verifier_constant_pool_004.abc";
    GenerateModifiedAbc(buffer, target_file_name);
    base_file.close();

    {
        panda::verifier::Verifier ver {target_file_name};
        EXPECT_FALSE(ver.VerifyConstantPool());
    }
}

/**
* @tc.name: verifier_constant_pool_005
* @tc.desc: Verify the method content of the abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_005, TestSize.Level1)
{
    const std::string base_file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    {
        panda::verifier::Verifier ver {base_file_name};
        EXPECT_TRUE(ver.VerifyConstantPool());
    }
    std::ifstream base_file(base_file_name, std::ios::binary);
    EXPECT_TRUE(base_file.is_open());

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(base_file), {});
    
    std::vector<uint8_t> method_content = {0x02};
    
    uint32_t method_id = 0x000e; // The known method id in the abc file

    buffer[method_id] = static_cast<unsigned char>(method_content[0]);

    const std::string target_file_name = GRAPH_TEST_ABC_DIR "verifier_constant_pool_005.abc";
    GenerateModifiedAbc(buffer, target_file_name);
    base_file.close();

    {
        panda::verifier::Verifier ver {target_file_name};
        EXPECT_FALSE(ver.Verify());
    }
}

/**
* @tc.name: verifier_constant_pool_006
* @tc.desc: Verify the literal content of the abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_006, TestSize.Level1)
{
    const std::string base_file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    {
        panda::verifier::Verifier ver {base_file_name};
        EXPECT_TRUE(ver.VerifyConstantPool());
    }
    std::ifstream base_file(base_file_name, std::ios::binary);
    EXPECT_TRUE(base_file.is_open());

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(base_file), {});
    
    std::vector<uint8_t> literal_content = {0xcf};
    
    uint32_t literal_id = 0x28b; // The known literal id in the abc file

    buffer[literal_id] = static_cast<unsigned char>(literal_content[0]);

    const std::string target_file_name = GRAPH_TEST_ABC_DIR "verifier_constant_pool_006.abc";
    GenerateModifiedAbc(buffer, target_file_name);
    base_file.close();

    {
        panda::verifier::Verifier ver {target_file_name};
        EXPECT_FALSE(ver.Verify());
    }
}

/**
* @tc.name: verifier_constant_pool_007
* @tc.desc: Verify the string content of the abc file.
* @tc.type: FUNC
* @tc.require: file path and name
*/
HWTEST_F(VerifierConstantPool, verifier_constant_pool_007, TestSize.Level1)
{
    const std::string base_file_name = GRAPH_TEST_ABC_DIR "test_constant_pool.abc";
    {
        panda::verifier::Verifier ver {base_file_name};
        EXPECT_TRUE(ver.VerifyConstantPool());
    }
    std::ifstream base_file(base_file_name, std::ios::binary);
    EXPECT_TRUE(base_file.is_open());

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(base_file), {});
    
    std::vector<uint8_t> string_content = {0x4a};
    
    uint32_t string_id = 0x000c; // The known string id in the abc file

    buffer[string_id] = static_cast<unsigned char>(string_content[0]);

    const std::string target_file_name = GRAPH_TEST_ABC_DIR "verifier_constant_pool_007.abc";
    GenerateModifiedAbc(buffer, target_file_name);
    base_file.close();

    {
        panda::verifier::Verifier ver {target_file_name};
        EXPECT_FALSE(ver.Verify());
    }
}

}; // namespace panda::verifier
