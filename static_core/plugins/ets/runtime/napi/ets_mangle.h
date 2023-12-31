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
#ifndef PANDA_PLUGINS_ETS_RUNTIME_ETS_MANGLE_H_
#define PANDA_PLUGINS_ETS_RUNTIME_ETS_MANGLE_H_

#include <iostream>

namespace panda::ets {

std::string MangleString(const std::string &name);

std::string MangleMethodName(const std::string &class_name, const std::string &method_name);

std::string MangleMethodNameWithSignature(const std::string &mangled_name, const std::string &signature);

}  // namespace panda::ets

#endif  // PANDA_PLUGINS_ETS_RUNTIME_ETS_MANGLE_H_