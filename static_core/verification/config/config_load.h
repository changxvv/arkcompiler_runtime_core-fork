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

#ifndef PANDA_VERIFICATION_CONFIG_CONFIG_LOAD_H
#define PANDA_VERIFICATION_CONFIG_CONFIG_LOAD_H

#include <string>

#include "verification/public.h"

namespace panda::verifier::config {
bool LoadConfig(Config *cfg, std::string_view filename);
}  // namespace panda::verifier::config

#endif  // PANDA_VERIFICATION_CONFIG_CONFIG_LOAD_H
