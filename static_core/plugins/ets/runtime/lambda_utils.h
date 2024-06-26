/**
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

#ifndef PANDA_PLUGINS_ETS_RUNTIME_LAMBDA_UTILS_H_
#define PANDA_PLUGINS_ETS_RUNTIME_LAMBDA_UTILS_H_

#include "libpandabase/macros.h"
#include "plugins/ets/runtime/ets_coroutine.h"
#include "plugins/ets/runtime/types/ets_object.h"

namespace ark::ets {

class LambdaUtils {
public:
    PANDA_PUBLIC_API static void InvokeVoid(EtsCoroutine *coro, EtsObject *lambda);

    NO_COPY_SEMANTIC(LambdaUtils);
    NO_MOVE_SEMANTIC(LambdaUtils);

private:
    LambdaUtils() = default;
    ~LambdaUtils() = default;
};
}  // namespace ark::ets

#endif  // PANDA_PLUGINS_ETS_RUNTIME_LAMBDA_UTILS_H_
