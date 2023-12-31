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

#include <gtest/gtest.h>
#include "ets_interop_js_gtest.h"

namespace panda::ets::interop::js::testing {

class EtsInteropJsDynamicWithLambda : public EtsInteropTest {};

TEST_F(EtsInteropJsDynamicWithLambda, TestArgs0)
{
    auto ret = CallEtsMethod<uint32_t>("TestArgs0");
    ASSERT_EQ(ret, 25);
}

TEST_F(EtsInteropJsDynamicWithLambda, TestArgs1)
{
    auto ret = CallEtsMethod<uint32_t>("TestArgs1");
    ASSERT_EQ(ret, 25);
}

TEST_F(EtsInteropJsDynamicWithLambda, TestArgs2)
{
    auto ret = CallEtsMethod<uint32_t>("TestArgs2");
    ASSERT_EQ(ret, 25);
}

// NOTE: vpukhov. re-implement proxy creation by erased type in runtime!
// NOTE: vpukhov. re-enable #14501
TEST_F(EtsInteropJsDynamicWithLambda, DISABLED_TestLambdaJSValue)
{
    auto ret = CallEtsMethod<uint32_t>("TestLambdaJSValue");
    ASSERT_EQ(ret, 25);
}

// NOTE: vpukhov. re-enable #14501
TEST_F(EtsInteropJsDynamicWithLambda, DISABLED_TestLambdaJSValueCast)
{
    auto ret = CallEtsMethod<uint32_t>("TestLambdaJSValueCast");
    ASSERT_EQ(ret, 25);
}

// NOTE: vpukhov. re-enable #14501
TEST_F(EtsInteropJsDynamicWithLambda, DISABLED_TestLambdaJSValueCache)
{
    auto ret = CallEtsMethod<uint32_t>("TestLambdaJSValueCache");
    ASSERT_EQ(ret, 25);
}

// NOTE(itrubachev) this test can be enabled after fixing NOTE in checker::Type *TSAsExpression::Check in es2panda
TEST_F(EtsInteropJsDynamicWithLambda, DISABLED_TestLambdaJSValueCastCallAsArgument)
{
    auto ret = CallEtsMethod<uint32_t>("TestLambdaJSValueCastCallAsArgument");
    ASSERT_EQ(ret, 25);
}

}  // namespace panda::ets::interop::js::testing
