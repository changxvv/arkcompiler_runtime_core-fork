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

#include <cstdlib>
#include "ets_intrinsics_helpers.h"
#include "include/mem/panda_string.h"
#include "types/ets_field.h"
#include "types/ets_string.h"

namespace panda::ets::intrinsics::helpers {

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define RETURN_IF_CONVERSION_END(p, end, result) \
    if ((p) == (end)) {                          \
        return (result);                         \
    }

double StringToDouble(const uint8_t *start, const uint8_t *end, uint8_t radix, uint32_t flags)
{
    if (IsEmptyString(start, end)) {
        return NAN_VALUE;
    }

    radix = 0;
    auto p = const_cast<uint8_t *>(start);

    // 1. skip space and line terminal
    if (!GotoNonspace(&p, end)) {
        return 0.0;
    }

    // 2. get number sign
    Sign sign = Sign::NONE;
    if (*p == '+') {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
        sign = Sign::POS;
    } else if (*p == '-') {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
        sign = Sign::NEG;
    }
    bool ignore_trailing = (flags & flags::IGNORE_TRAILING) != 0;

    // 3. judge Infinity
    static const char INF[] = "Infinity";  // NOLINT(modernize-avoid-c-arrays)
    if (*p == INF[0]) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (const char *i = &INF[1]; *i != '\0'; ++i) {
            if (++p == end || *p != *i) {
                return NAN_VALUE;
            }
        }
        ++p;
        if (!ignore_trailing && GotoNonspace(&p, end)) {
            return NAN_VALUE;
        }
        return sign == Sign::NEG ? -POSITIVE_INFINITY : POSITIVE_INFINITY;
    }

    // 4. get number radix
    bool leading_zero = false;
    bool prefix_radix = false;
    if (*p == '0' && radix == 0) {
        RETURN_IF_CONVERSION_END(++p, end, SignedZero(sign));
        if (*p == 'x' || *p == 'X') {
            if ((flags & flags::ALLOW_HEX) == 0) {
                return ignore_trailing ? SignedZero(sign) : NAN_VALUE;
            }
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
            if (sign != Sign::NONE) {
                return NAN_VALUE;
            }
            prefix_radix = true;
            radix = HEXADECIMAL;
        } else if (*p == 'o' || *p == 'O') {
            if ((flags & flags::ALLOW_OCTAL) == 0) {
                return ignore_trailing ? SignedZero(sign) : NAN_VALUE;
            }
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
            if (sign != Sign::NONE) {
                return NAN_VALUE;
            }
            prefix_radix = true;
            radix = OCTAL;
        } else if (*p == 'b' || *p == 'B') {
            if ((flags & flags::ALLOW_BINARY) == 0) {
                return ignore_trailing ? SignedZero(sign) : NAN_VALUE;
            }
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
            if (sign != Sign::NONE) {
                return NAN_VALUE;
            }
            prefix_radix = true;
            radix = BINARY;
        } else {
            leading_zero = true;
        }
    }

    if (radix == 0) {
        radix = DECIMAL;
    }
    auto p_start = p;
    // 5. skip leading '0'
    while (*p == '0') {
        RETURN_IF_CONVERSION_END(++p, end, SignedZero(sign));
        leading_zero = true;
    }
    // 6. parse to number
    uint64_t int_number = 0;
    uint64_t number_max = (UINT64_MAX - (radix - 1)) / radix;
    int digits = 0;
    int exponent = 0;
    do {
        uint8_t c = ToDigit(*p);
        if (c >= radix) {
            if (!prefix_radix || ignore_trailing || (p_start != p && !GotoNonspace(&p, end))) {
                break;
            }
            // "0b" "0x1.2" "0b1e2" ...
            return NAN_VALUE;
        }
        ++digits;
        if (int_number < number_max) {
            int_number = int_number * radix + c;
        } else {
            ++exponent;
        }
    } while (++p != end);

    auto number = static_cast<double>(int_number);
    if (sign == Sign::NEG) {
        if (number == 0) {
            number = -0.0;
        } else {
            number = -number;
        }
    }

    // 7. deal with other radix except DECIMAL
    if (p == end || radix != DECIMAL) {
        if ((digits == 0 && !leading_zero) || (p != end && !ignore_trailing && GotoNonspace(&p, end))) {
            // no digits there, like "0x", "0xh", or error trailing of "0x3q"
            return NAN_VALUE;
        }
        return number * std::pow(radix, exponent);
    }

    // 8. parse '.'
    if (radix == DECIMAL && *p == '.') {
        RETURN_IF_CONVERSION_END(++p, end, (digits > 0) ? (number * std::pow(radix, exponent)) : NAN_VALUE);
        while (ToDigit(*p) < radix) {
            --exponent;
            ++digits;
            if (++p == end) {
                break;
            }
        }
    }
    if (digits == 0 && !leading_zero) {
        // no digits there, like ".", "sss", or ".e1"
        return NAN_VALUE;
    }
    auto p_end = p;

    // 9. parse 'e/E' with '+/-'
    char exponent_sign = '+';
    int additional_exponent = 0;
    constexpr int MAX_EXPONENT = INT32_MAX / 2;
    if (radix == DECIMAL && (p != end && (*p == 'e' || *p == 'E'))) {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);

        // 10. parse exponent number
        if (*p == '+' || *p == '-') {
            exponent_sign = static_cast<char>(*p);
            RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
        }
        uint8_t digit;
        while ((digit = ToDigit(*p)) < radix) {
            if (additional_exponent > static_cast<int>(MAX_EXPONENT / radix)) {
                additional_exponent = MAX_EXPONENT;
            } else {
                additional_exponent = additional_exponent * static_cast<int>(radix) + static_cast<int>(digit);
            }
            if (++p == end) {
                break;
            }
        }
    }
    exponent += (exponent_sign == '-' ? -additional_exponent : additional_exponent);
    if (!ignore_trailing && GotoNonspace(&p, end)) {
        return NAN_VALUE;
    }

    // 10. build StringNumericLiteral string
    PandaString buffer;
    if (sign == Sign::NEG) {
        buffer += "-";
    }
    for (uint8_t *i = p_start; i < p_end; ++i) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (*i != static_cast<uint8_t>('.')) {
            buffer += *i;
        }
    }

    // 11. convert none-prefix radix string
    return Strtod(buffer.c_str(), exponent, radix);
}

double StringToDoubleWithRadix(const uint8_t *start, const uint8_t *end, int radix)
{
    auto p = const_cast<uint8_t *>(start);
    // 1. skip space and line terminal
    if (!GotoNonspace(&p, end)) {
        return NAN_VALUE;
    }

    // 2. sign bit
    bool negative = false;
    if (*p == '-') {
        negative = true;
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
    } else if (*p == '+') {
        RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
    }
    // 3. 0x or 0X
    bool strip_prefix = true;
    // 4. If R != 0, then
    //     a. If R < 2 or R > 36, return NaN.
    //     b. If R != 16, let stripPrefix be false.
    if (radix != 0) {
        if (radix < MIN_RADIX || radix > MAX_RADIX) {
            return NAN_VALUE;
        }
        if (radix != HEXADECIMAL) {
            strip_prefix = false;
        }
    } else {
        radix = DECIMAL;
    }
    int size = 0;
    if (strip_prefix) {
        if (*p == '0') {
            size++;
            if (++p != end && (*p == 'x' || *p == 'X')) {
                RETURN_IF_CONVERSION_END(++p, end, NAN_VALUE);
                radix = HEXADECIMAL;
            }
        }
    }

    double result = 0;
    bool is_done = false;
    do {
        double part = 0;
        uint32_t multiplier = 1;
        for (; p != end; ++p) {
            // The maximum value to ensure that uint32_t will not overflow
            const uint32_t max_multiper = 0xffffffffU / 36;
            uint32_t m = multiplier * static_cast<uint32_t>(radix);
            if (m > max_multiper) {
                break;
            }

            int current_bit = static_cast<int>(ToDigit(*p));
            if (current_bit >= radix) {
                is_done = true;
                break;
            }
            size++;
            part = part * radix + current_bit;
            multiplier = m;
        }
        result = result * multiplier + part;
        if (is_done) {
            break;
        }
    } while (p != end);

    if (size == 0) {
        return NAN_VALUE;
    }

    return negative ? -result : result;
}

EtsString *DoubleToExponential(double number, int digit)
{
    PandaStringStream ss;
    if (digit < 0) {
        ss << std::setiosflags(std::ios::scientific) << std::setprecision(MAX_PRECISION) << number;
    } else {
        ss << std::setiosflags(std::ios::scientific) << std::setprecision(digit) << number;
    }
    PandaString result = ss.str();
    size_t found = result.find_last_of('e');
    if (found != PandaString::npos && found < result.size() - 2 && result[found + 2] == '0') {
        result.erase(found + 2, 1);  // 2:offset of e
    }
    if (digit < 0) {
        size_t end = found;
        while (--found > 0) {
            if (result[found] != '0') {
                break;
            }
        }
        if (result[found] == '.') {
            found--;
        }
        if (found < end - 1) {
            result.erase(found + 1, end - found - 1);
        }
    }
    return EtsString::CreateFromMUtf8(result.c_str());
}

EtsString *DoubleToFixed(double number, int digit)
{
    PandaStringStream ss;
    ss << std::setiosflags(std::ios::fixed) << std::setprecision(digit) << number;
    return EtsString::CreateFromMUtf8(ss.str().c_str());
}

EtsString *DoubleToPrecision(double number, int digit)
{
    if (number == 0.0) {
        return DoubleToFixed(number, digit - 1);
    }
    PandaStringStream ss;
    double positive_number = number > 0 ? number : -number;
    int log_digit = std::floor(log10(positive_number));
    int radix_digit = digit - log_digit - 1;
    const int max_exponent_digit = 6;
    if ((log_digit >= 0 && radix_digit >= 0) || (log_digit < 0 && radix_digit <= max_exponent_digit)) {
        return DoubleToFixed(number, std::abs(radix_digit));
    }
    return DoubleToExponential(number, digit - 1);
}

double GetStdDoubleArgument(ObjectHeader *obj)
{
    auto *cls = obj->ClassAddr<Class>();

    // Assume std.core.Double has only one `double` field
    ASSERT(cls->GetInstanceFields().size() == 1);

    Field &field_val = cls->GetInstanceFields()[0];

    ASSERT(field_val.GetTypeId() == panda_file::Type::TypeId::F64);

    size_t offset = field_val.GetOffset();
    return obj->GetFieldPrimitive<double>(offset);
}

}  // namespace panda::ets::intrinsics::helpers
