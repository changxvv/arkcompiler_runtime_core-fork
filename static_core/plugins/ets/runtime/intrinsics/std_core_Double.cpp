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

#include <cstdint>
#include <limits>
#include "include/mem/panda_string.h"
#include "intrinsics.h"
#include "plugins/ets/runtime/types/ets_string.h"
#include "plugins/ets/runtime/intrinsics/helpers/ets_intrinsics_helpers.h"
#include "unicode/locid.h"
#include "unicode/coll.h"
#include "unicode/numberformatter.h"
#include "unicode/unistr.h"
#include "utils/utf.h"

namespace panda::ets::intrinsics {

EtsString *StdCoreDoubleToString(double number, int radix)
{
    return helpers::FpToString(number, radix);
}

EtsString *StdCoreDoubleToLocaleString(ObjectHeader *obj, EtsString *locale)
{
    ASSERT(obj != nullptr && locale != nullptr);
    icu::Locale loc;
    UErrorCode status = U_ZERO_ERROR;
    PandaVector<uint8_t> buf;
    std::string_view loc_tag = locale->ConvertToStringView(&buf);
    icu::StringPiece sp {loc_tag.data(), static_cast<int32_t>(loc_tag.size())};
    loc = icu::Locale::forLanguageTag(sp, status);

    if (UNLIKELY(U_FAILURE(status))) {
        std::string message = "Language tag '" + std::string(loc_tag) + "' is invalid or not supported";
        ThrowEtsException(EtsCoroutine::GetCurrent(), panda_file_items::class_descriptors::RANGE_ERROR, message);
        return nullptr;
    }

    double obj_value = helpers::GetStdDoubleArgument(obj);

    icu::number::LocalizedNumberFormatter loc_num_fmt = icu::number::NumberFormatter::withLocale(loc);
    icu::number::FormattedNumber fmt_num = loc_num_fmt.formatDouble(obj_value, status);

    if (UNLIKELY(U_FAILURE(status))) {
        std::string message = "Unable to convert " + std::to_string(obj_value) + " to locale " + std::string(loc_tag);
        ThrowEtsException(EtsCoroutine::GetCurrent(), panda_file_items::class_descriptors::RUNTIME_EXCEPTION, message);
        return nullptr;
    }

    icu::UnicodeString uni_str = fmt_num.toString(status);
    return EtsString::CreateFromUtf16(reinterpret_cast<const uint16_t *>(uni_str.getBuffer()), uni_str.length());
}

double StdCoreDoubleParseFloat(EtsString *s)
{
    if (UNLIKELY(s->IsUtf16())) {
        size_t len = utf::Utf16ToUtf8Size(s->GetDataUtf16(), s->GetUtf16Length()) - 1;
        PandaVector<uint8_t> buf(len);
        len = utf::ConvertRegionUtf16ToUtf8(s->GetDataUtf16(), buf.data(), s->GetLength(), len, 0);

        Span<uint8_t> str = Span<uint8_t>(buf.data(), len);
        return helpers::StringToDouble(str.begin(), str.end(), 0, helpers::flags::IGNORE_TRAILING);
    }

    Span<uint8_t> str = Span<uint8_t>(s->GetDataMUtf8(), s->GetMUtf8Length() - 1);
    return helpers::StringToDouble(str.begin(), str.end(), 0, helpers::flags::IGNORE_TRAILING);
}

double StdCoreDoubleParseInt(EtsString *s, int32_t radix)
{
    if (UNLIKELY(s->IsUtf16())) {
        size_t len = utf::Utf16ToUtf8Size(s->GetDataUtf16(), s->GetUtf16Length()) - 1;
        PandaVector<uint8_t> buf(len);
        len = utf::ConvertRegionUtf16ToUtf8(s->GetDataUtf16(), buf.data(), s->GetLength(), len, 0);

        Span<uint8_t> str = Span<uint8_t>(buf.data(), len);
        return std::abs(helpers::StringToDoubleWithRadix(str.begin(), str.end(), radix));
    }

    Span<uint8_t> str = Span<uint8_t>(s->GetDataMUtf8(), s->GetMUtf8Length() - 1);
    return std::trunc(helpers::StringToDoubleWithRadix(str.begin(), str.end(), radix));
}

EtsString *StdCoreDoubleToExponential(ObjectHeader *obj, double d)
{
    double obj_value = helpers::GetStdDoubleArgument(obj);

    // If x is NaN, return the String "NaN".
    if (std::isnan(obj_value)) {
        return EtsString::CreateFromMUtf8("NaN");
    }
    // If x < 0, then
    //    a. Let s be "-".
    //    b. Let x = –x.
    // If x = +infinity, then
    //    a. Return the concatenation of the Strings s and "Infinity".
    if (!std::isfinite(obj_value)) {
        if (obj_value < 0) {
            return EtsString::CreateFromMUtf8("-Infinity");
        }
        return EtsString::CreateFromMUtf8("Infinity");
    }

    // truncate the arg val
    double digit_abs = std::isnan(d) ? 0 : d;
    digit_abs = std::abs((digit_abs >= 0) ? std::floor(digit_abs) : std::ceil(digit_abs));

    // Check range
    if (UNLIKELY(digit_abs > helpers::MAX_FRACTION || digit_abs < helpers::MIN_FRACTION)) {
        ThrowEtsException(EtsCoroutine::GetCurrent(),
                          panda_file_items::class_descriptors::ARGUMENT_OUT_OF_RANGE_EXCEPTION,
                          "toExponential argument must be between 0 and 100");
        return nullptr;
    }

    return helpers::DoubleToExponential(obj_value, static_cast<int>(digit_abs));
}

EtsString *StdCoreDoubleToPrecision(ObjectHeader *obj, double d)
{
    double obj_value = helpers::GetStdDoubleArgument(obj);

    // If x is NaN, return the String "NaN".
    if (std::isnan(obj_value)) {
        return EtsString::CreateFromMUtf8("NaN");
    }
    // If x < 0, then
    //    a. Let s be "-".
    //    b. Let x = –x.
    // If x = +infinity, then
    //    a. Return the concatenation of the Strings s and "Infinity".
    if (!std::isfinite(obj_value)) {
        if (obj_value < 0) {
            return EtsString::CreateFromMUtf8("-Infinity");
        }
        return EtsString::CreateFromMUtf8("Infinity");
    }

    // truncate the arg val
    double digit_abs = std::isnan(d) ? 0 : d;
    digit_abs = std::abs((digit_abs >= 0) ? std::floor(digit_abs) : std::ceil(digit_abs));

    // Check range
    if (UNLIKELY(digit_abs > helpers::MAX_FRACTION || digit_abs < helpers::MIN_FRACTION + 1)) {
        ThrowEtsException(EtsCoroutine::GetCurrent(),
                          panda_file_items::class_descriptors::ARGUMENT_OUT_OF_RANGE_EXCEPTION,
                          "toPrecision argument must be between 1 and 100");
        return nullptr;
    }

    return helpers::DoubleToPrecision(obj_value, static_cast<int>(digit_abs));
}

EtsString *StdCoreDoubleToFixed(ObjectHeader *obj, double d)
{
    double obj_value = helpers::GetStdDoubleArgument(obj);

    // If x is NaN, return the String "NaN".
    if (std::isnan(obj_value)) {
        return EtsString::CreateFromMUtf8("NaN");
    }
    // If x < 0, then
    //    a. Let s be "-".
    //    b. Let x = –x.
    // If x = +infinity, then
    //    a. Return the concatenation of the Strings s and "Infinity".
    if (!std::isfinite(obj_value)) {
        if (obj_value < 0) {
            return EtsString::CreateFromMUtf8("-Infinity");
        }
        return EtsString::CreateFromMUtf8("Infinity");
    }

    // truncate the arg val
    double digit_abs = std::isnan(d) ? 0 : d;
    digit_abs = std::abs((digit_abs >= 0) ? std::floor(digit_abs) : std::ceil(digit_abs));

    // Check range
    if (UNLIKELY(digit_abs > helpers::MAX_FRACTION || digit_abs < helpers::MIN_FRACTION)) {
        ThrowEtsException(EtsCoroutine::GetCurrent(),
                          panda_file_items::class_descriptors::ARGUMENT_OUT_OF_RANGE_EXCEPTION,
                          "toFixed argument must be between 0 and 100");
        return nullptr;
    }

    return helpers::DoubleToFixed(obj_value, static_cast<int>(digit_abs));
}

extern "C" EtsBoolean StdCoreDoubleIsNan(double v)
{
    return ToEtsBoolean(v != v);
}

extern "C" EtsBoolean StdCoreDoubleIsFinite(double v)
{
    static const double POSITIVE_INFINITY = 1.0 / 0.0;
    static const double NEGATIVE_INFINITY = -1.0 / 0.0;

    return ToEtsBoolean(v == v && v != POSITIVE_INFINITY && v != NEGATIVE_INFINITY);
}

extern "C" EtsDouble StdCoreDoubleBitCastFromLong(EtsLong i)
{
    return bit_cast<EtsDouble>(i);
}

extern "C" EtsLong StdCoreDoubleBitCastToLong(EtsDouble f)
{
    return bit_cast<EtsLong>(f);
}

static inline bool IsInteger(double v)
{
    return std::isfinite(v) && (std::fabs(v - std::trunc(v)) <= std::numeric_limits<double>::epsilon());
}

extern "C" EtsBoolean StdCoreDoubleIsInteger(double v)
{
    return ToEtsBoolean(IsInteger(v));
}

/*
 * In ETS Double.isSafeInteger returns (Double.isInteger(v) && (abs(v) <= Double.MAX_SAFE_INTEGER)).
 * MAX_SAFE_INTEGER is a max integer value that can be used as a double without losing precision.
 */
extern "C" EtsBoolean StdCoreDoubleIsSafeInteger(double v)
{
    return ToEtsBoolean(IsInteger(v) && (std::fabs(v) <= helpers::MaxSafeInteger<double>()));
}

}  // namespace panda::ets::intrinsics
