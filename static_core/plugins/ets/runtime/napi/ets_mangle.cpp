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
#include <cstdio>
#include <securec.h>
#include "plugins/ets/runtime/napi/ets_napi.h"
#include "libpandabase/utils/utf.h"
#include "plugins/ets/runtime/napi/ets_mangle.h"

namespace panda::ets {

std::string MangleString(const std::string &name)
{
    std::stringstream res;
    const uint8_t *utf8 = panda::utf::CStringAsMutf8(name.c_str());
    while (*utf8 != '\0') {
        auto [ch, len] = panda::utf::ConvertMUtf8ToUtf16Pair(utf8);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        utf8 += len;
        if (ch == '.' || ch == '/') {
            res << "_";
        } else if (ch == '_') {
            res << "_1";
        } else if (ch == ';') {
            res << "_2";
        } else if (ch == '[') {
            res << "_3";
        } else if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
            res << static_cast<char>(ch);
        } else {
            // append _0xxxx, where xxxx - unicode of symbol
            constexpr int BUF_LEN = 16;
            std::array<char, BUF_LEN> buf = {0};
            auto [p_hi, p_lo] = panda::utf::SplitUtf16Pair(ch);
            if (p_hi == 0) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                snprintf_s(buf.data(), BUF_LEN, BUF_LEN - 1, "_0%04x", p_lo);
            } else {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                snprintf_s(buf.data(), BUF_LEN, BUF_LEN - 1, "_0%04x_0%04x", p_lo, p_hi);
            }
            res << buf.data();
        }
    }
    return res.str();
}

std::string MangleMethodName(const std::string &class_name, const std::string &method_name)
{
    std::string name(class_name);
    if (name[0] == 'L') {
        name = name.substr(1, name.size() - 2);
    }
    name.append(".");
    name.append(method_name);
    name = MangleString(name);
    name.insert(0, "ETS_");
    return name;
}

std::string MangleMethodNameWithSignature(const std::string &mangled_name, const std::string &signature)
{
    std::stringstream res;
    res << mangled_name << "__" << MangleString(signature);
    return res.str();
}

}  // namespace panda::ets
