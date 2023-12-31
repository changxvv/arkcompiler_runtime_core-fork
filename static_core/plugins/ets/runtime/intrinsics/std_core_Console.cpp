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

#include <iostream>
#include <string_view>
#include "plugins/ets/runtime/ets_vm.h"
#include "plugins/ets/runtime/types/ets_string.h"
#include "plugins/ets/runtime/types/ets_void.h"
#include "libpandabase/utils/utf.h"

#include "intrinsics.h"

namespace panda::ets::intrinsics {

extern "C" EtsVoid *StdConsolePrintln(ObjectHeader *header [[maybe_unused]])
{
    std::cout << std::endl;
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintBool([[maybe_unused]] ObjectHeader *header, uint8_t b)
{
    if (b != 0U) {
        std::cout << "true";
    } else {
        std::cout << "false";
    }
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintChar([[maybe_unused]] ObjectHeader *header, uint16_t c)
{
    const utf::Utf8Char utf8_ch = utf::ConvertUtf16ToUtf8(c, 0, false);
    std::cout << std::string_view(reinterpret_cast<const char *>(utf8_ch.ch.data()), utf8_ch.n);
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintString([[maybe_unused]] ObjectHeader *header, EtsString *str)
{
    panda::intrinsics::PrintString(str->GetCoreType());
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintI32([[maybe_unused]] ObjectHeader *header, int32_t v)
{
    panda::intrinsics::PrintI32(v);
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintI16([[maybe_unused]] ObjectHeader *header, int16_t v)
{
    panda::intrinsics::PrintI32(v);
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintI8([[maybe_unused]] ObjectHeader *header, int8_t v)
{
    panda::intrinsics::PrintI32(v);
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintI64([[maybe_unused]] ObjectHeader *header, int64_t v)
{
    panda::intrinsics::PrintI64(v);
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintF32([[maybe_unused]] ObjectHeader *header, float v)
{
    panda::intrinsics::PrintF32(v);
    return EtsVoid::GetInstance();
}

extern "C" EtsVoid *StdConsolePrintF64([[maybe_unused]] ObjectHeader *header, double v)
{
    panda::intrinsics::PrintF64(v);
    return EtsVoid::GetInstance();
}

}  // namespace panda::ets::intrinsics
