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

#ifndef PANDA_PLUGINS_ETS_RUNTIME_TS2ETS_TS2ETS_COPY_H_
#define PANDA_PLUGINS_ETS_RUNTIME_TS2ETS_TS2ETS_COPY_H_

#include <node_api.h>

namespace panda::ets::interop::js {

napi_value InvokeEtsMethodImpl(napi_env env, napi_value *jsargv, uint32_t jsargc, bool doClscheck);

}  // namespace panda::ets::interop::js

#endif  // !PANDA_PLUGINS_ETS_RUNTIME_TS2ETS_TS2ETS_COPY_H_
