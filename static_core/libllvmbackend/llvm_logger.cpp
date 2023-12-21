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

#include "llvm_logger.h"

namespace panda::llvmbackend {
std::bitset<LLVMLoggerComponents::LOG_COMPONENTS_NUM> LLVMLogger::components_(0);

// clang-format off
void LLVMLogger::SetComponents(const std::vector<std::string>& args)
{
    components_.reset();
    for (const auto& arg : args) {
        if (arg == "all") {
            components_.set();
            break;
        }
        if (arg == "none") {
            components_.reset();
            break;
        }
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DEF(COMPONENT, NAME)                 \
            if ((NAME) == arg) {               \
                components_.set(COMPONENT);  \
                continue;                    \
            }
        LLVM_LOG_COMPONENTS(DEF)
#undef DEF

        UNREACHABLE();
    }
}
// clang-format on

}  // namespace panda::llvmbackend