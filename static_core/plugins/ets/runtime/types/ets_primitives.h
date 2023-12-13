/**
 * Copyright (c) 2021-2024 Huawei Device Co., Ltd.
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
#ifndef PANDA_RUNTIME_ETS_TYPES_ETS_PRIMITIVES_H_
#define PANDA_RUNTIME_ETS_TYPES_ETS_PRIMITIVES_H_

#include <cstdint>
#include <type_traits>

namespace ark::ets {
// Primitive types association got from runtime/class_linker.cpp:InitializeFields()
using EtsVoid = void;
using EtsBoolean = uint8_t;
using EtsByte = int8_t;
using EtsChar = uint16_t;
using EtsShort = int16_t;
using EtsInt = int32_t;
using EtsLong = int64_t;
using EtsFloat = float;
using EtsDouble = double;

constexpr EtsBoolean ToEtsBoolean(bool b)
{
    return static_cast<EtsBoolean>(b);
}

}  // namespace ark::ets

#endif  // PANDA_RUNTIME_ETS_TYPES_ETS_PRIMITIVES_H_
