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

#ifndef PANDA_RUNTIME_CORE_CORE_VTABLE_BUILDER_H_
#define PANDA_RUNTIME_CORE_CORE_VTABLE_BUILDER_H_

#include "runtime/include/vtable_builder.h"

namespace ark {

struct CoreVTableSearchBySignature {
    bool operator()(const MethodInfo &info1, const MethodInfo &info2) const
    {
        return info1.IsEqualByNameAndSignature(info2);
    }
};

struct CoreVTableOverridePred {
    std::pair<bool, size_t> operator()([[maybe_unused]] const MethodInfo &info1,
                                       [[maybe_unused]] const MethodInfo &info2) const
    {
        return std::pair<bool, size_t>(true, MethodInfo::INVALID_METHOD_IDX);
    }
};

using CoreVTableBuilder = VTableBuilderImpl<CoreVTableSearchBySignature, CoreVTableOverridePred>;

}  // namespace ark

#endif  // PANDA_RUNTIME_CORE_CORE_VTABLE_BUILDER_H_
