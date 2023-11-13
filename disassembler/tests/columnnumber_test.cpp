#include <gtest/gtest.h>
#include <string>
#include "../disassembler.h"

using namespace testing::ext;
namespace panda::disasm{
class DisasmTest : public testing::Test {
public:
    static void SetUpTestCase(void) {};
    static void TearDownTestCase(void) {};
    void SetUp() {};
    void TearDown() {};
};

HWTEST_F(DisasmTest, disasm_column_numbers1, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "disasm_column_numbers1.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::string number_str = "8484840";
    std::string column_number_str = disasm.GetColumnNumber();
    EXPECT_TRUE(number_str == column_number_str);
}

HWTEST_F(DisasmTest, disasm_column_numbers2, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "disasm_column_numbers2.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::string number_str = "12840";
    std::string column_number_str = disasm.GetColumnNumber();
    EXPECT_TRUE(number_str == column_number_str);
}

HWTEST_F(DisasmTest, disasm_column_numbers3, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "disasm_column_numbers3.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::string number_str = "1216128484840";
    std::string column_number_str = disasm.GetColumnNumber();
    EXPECT_TRUE(number_str == column_number_str);
}

HWTEST_F(DisasmTest, disasm_column_numbers4, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "disasm_column_numbers4.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::string number_str = "1582081981581461460";
    std::string column_number_str = disasm.GetColumnNumber();
    EXPECT_TRUE(number_str == column_number_str);
}

HWTEST_F(DisasmTest, disasm_column_numbers5, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "disasm_column_numbers5.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::string number_str = "12161281281216128114485151510";
    std::string column_number_str = disasm.GetColumnNumber();
    EXPECT_TRUE(number_str == column_number_str);
}

HWTEST_F(DisasmTest, disasm_column_numbers6, TestSize.Level1)
{
    const std::string file_name = GRAPH_TEST_ABC_DIR "disasm_column_numbers6.abc";
    panda::disasm::Disassembler disasm {};
    disasm.Disassemble(file_name, false, false);
    disasm.CollectInfo();
    std::string number_str = "8412812161281216128114415820819815814614640";
    std::string column_number_str = disasm.GetColumnNumber();
    EXPECT_TRUE(number_str == column_number_str);
}
}
