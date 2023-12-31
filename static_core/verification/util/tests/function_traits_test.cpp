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

#include "util/function_traits.h"

#include "util/tests/verifier_test.h"

#include <gtest/gtest.h>

namespace panda::verifier::test {

struct SquareSum {
    int operator()(int x, int y) const
    {
        return (x + y) * (x + y);
    }
};

struct SquareDiversity {
    int operator()(int x, int y) const
    {
        return (x - y) * (x - y);
    }
};

struct MultByMod {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    int mod;
    explicit MultByMod(int module) : mod {module} {}
    int operator()(int x, int y) const
    {
        return (x * y) % mod;
    }
};

TEST_F(VerifierTest, function_traits)
{
    SquareSum sq_sum;
    SquareDiversity sq_div;
    NAry<SquareSum> op_s_sum {sq_sum};
    NAry<SquareDiversity> op_s_div {sq_div};
    EXPECT_EQ(op_s_sum(2, 2), 16);
    EXPECT_EQ(op_s_div(2, 1), 1);
    EXPECT_EQ(op_s_sum(2, 1, 2), 121);
    EXPECT_EQ(op_s_div(2, 1, 2), 1);

    MultByMod mod5 {5};
    // NOLINTNEXTLINE(readability-magic-numbers)
    MultByMod mod10 {10};
    NAry<MultByMod> op_mult_mod5 {mod5};
    NAry<MultByMod> op_mult_mod10 {mod10};
    EXPECT_EQ(op_mult_mod5(2, 4), 3);
    EXPECT_EQ(op_mult_mod10(2, 4), 8);
    EXPECT_EQ(op_mult_mod5(2, 4, 2), 1);
    EXPECT_EQ(op_mult_mod10(2, 4, 2), 6);
}

}  // namespace panda::verifier::test