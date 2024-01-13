/**
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

#include "plugins/ets/runtime/types/ets_box_primitive.h"
#include "plugins/ets/runtime/ets_class_linker_extension.h"

namespace panda::ets {
template <typename T>
EtsBoxPrimitive<T> *EtsBoxPrimitive<T>::Create(EtsCoroutine *coro, T value)
{
    auto *ext = coro->GetPandaVM()->GetClassLinker()->GetEtsClassLinkerExtension();
    Class *boxClass = nullptr;
    if constexpr (std::is_same<T, EtsBoolean>::value) {
        boxClass = ext->GetBoxBooleanClass();
    } else if constexpr (std::is_same<T, EtsByte>::value) {
        boxClass = ext->GetBoxByteClass();
    } else if constexpr (std::is_same<T, EtsChar>::value) {
        boxClass = ext->GetBoxCharClass();
    } else if constexpr (std::is_same<T, EtsShort>::value) {
        boxClass = ext->GetBoxShortClass();
    } else if constexpr (std::is_same<T, EtsInt>::value) {
        boxClass = ext->GetBoxIntClass();
    } else if constexpr (std::is_same<T, EtsLong>::value) {
        boxClass = ext->GetBoxLongClass();
    } else if constexpr (std::is_same<T, EtsFloat>::value) {
        boxClass = ext->GetBoxFloatClass();
    } else if constexpr (std::is_same<T, EtsDouble>::value) {
        boxClass = ext->GetBoxDoubleClass();
    }
    auto *instance = reinterpret_cast<EtsBoxPrimitive<T> *>(ObjectHeader::Create(coro, boxClass));
    instance->SetValue(value);
    return instance;
}
}  // namespace panda::ets
