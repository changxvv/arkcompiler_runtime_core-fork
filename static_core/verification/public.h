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

#ifndef PANDA_VERIFICATION_PUBLIC_H_
#define PANDA_VERIFICATION_PUBLIC_H_

#include "runtime/include/mem/allocator.h"
#include "runtime/include/method.h"
#include "runtime/include/runtime_options.h"
#include <functional>
#include <string_view>
namespace panda {
class ClassLinker;
}  // namespace panda

namespace panda::verifier {

using Config = struct Config;  // NOLINT(bugprone-forward-declaration-namespace)

Config *NewConfig();
bool LoadConfigFile(Config *config, std::string_view config_file_name);
void DestroyConfig(Config *config);

bool IsEnabled(Config const *config);
bool IsOnlyVerify(Config const *config);

using Service = struct Service;

Service *CreateService(Config const *config, panda::mem::InternalAllocatorPtr allocator, ClassLinker *linker,
                       std::string const &cache_file_name);
void DestroyService(Service *service, bool update_cache_file);

Config const *GetServiceConfig(Service const *service);

enum class Status { OK, FAILED, UNKNOWN };

PANDA_PUBLIC_API Status Verify(Service *service, panda::Method *method, VerificationMode mode);

}  // namespace panda::verifier

#endif  // PANDA_VERIFICATION_PUBLIC_H_
