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

#include "verification/public_internal.h"
#include "verification/verification_options.h"
#include "verification/config/process/config_process.h"
#include "verification/util/parser/parser.h"
#include "verification/util/struct_field.h"

#include "literal_parser.h"

#include "verifier_messages.h"

#include "runtime/include/method.h"
#include "runtime/include/runtime.h"
#include "runtime/include/mem/panda_containers.h"
#include "runtime/include/mem/panda_string.h"

#include "utils/logger.h"

#include <cstring>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace panda::verifier::debug {

namespace {

template <typename M>
PandaString GetKeys(const M &map)
{
    PandaString keys;
    for (const auto &p : map) {
        if (keys.empty()) {
            keys += "'";
            keys += p.first;
            keys += "'";
        } else {
            keys += ", '";
            keys += p.first;
            keys += "'";
        }
    }
    return keys;
}

}  // namespace

using panda::verifier::config::Section;

void RegisterConfigHandlerOptions(Config *dcfg)
{
    static const auto CONFIG_DEBUG_OPTIONS_VERIFIER = [](Config *config, const Section &section) {
        using BoolField = StructField<VerificationOptions, bool>;
        using Flags = PandaUnorderedMap<PandaString, BoolField>;
        using FlagsSection = PandaUnorderedMap<PandaString, Flags>;

        const FlagsSection flags = {
            {"show",
             {{"context", BoolField {offsetof(VerificationOptions, debug.show.context)}},
              {"reg-changes", BoolField {offsetof(VerificationOptions, debug.show.regChanges)}},
              {"typesystem", BoolField {offsetof(VerificationOptions, debug.show.typeSystem)}},
              {"status", BoolField {offsetof(VerificationOptions, show.status)}}}},
            {"allow",
             {{"undefined-class", BoolField {offsetof(VerificationOptions, debug.allow.undefinedClass)}},
              {"undefined-method", BoolField {offsetof(VerificationOptions, debug.allow.undefinedMethod)}},
              {"undefined-field", BoolField {offsetof(VerificationOptions, debug.allow.undefinedField)}},
              {"undefined-type", BoolField {offsetof(VerificationOptions, debug.allow.undefinedType)}},
              {"undefined-string", BoolField {offsetof(VerificationOptions, debug.allow.undefinedString)}},
              {"method-access-violation", BoolField {offsetof(VerificationOptions, debug.allow.methodAccessViolation)}},
              {"field-access-violation", BoolField {offsetof(VerificationOptions, debug.allow.fieldAccessViolation)}},
              {"wrong-subclassing-in-method-args",
               BoolField {offsetof(VerificationOptions, debug.allow.wrongSubclassingInMethodArgs)}},
              {"error-in-exception-handler",
               BoolField {offsetof(VerificationOptions, debug.allow.errorInExceptionHandler)}},
              {"permanent-runtime-exception",
               BoolField {offsetof(VerificationOptions, debug.allow.permanentRuntimeException)}}}}};

        auto &verifOpts = config->opts;
        for (const auto &s : section.sections) {
            if (flags.count(s.name) == 0) {
                LOG_VERIFIER_DEBUG_CONFIG_WRONG_OPTIONS_SECTION(s.name, GetKeys(flags));
                return false;
            }
            const auto &sectionFlags = flags.at(s.name);
            for (const auto &i : s.items) {
                PandaVector<PandaString> c;
                const char *start = i.c_str();
                const char *end = i.c_str() + i.length();  // NOLINT
                if (!LiteralsParser()(c, start, end)) {
                    LOG_VERIFIER_DEBUG_CONFIG_WRONG_OPTIONS_LINE(i);
                    return false;
                }
                if (!c.empty()) {
                    for (const auto &l : c) {
                        if (sectionFlags.count(l) == 0) {
                            LOG_VERIFIER_DEBUG_CONFIG_WRONG_OPTION_FOR_SECTION(l, s.name, GetKeys(sectionFlags));
                            return false;
                        }
                        sectionFlags.at(l).Of(verifOpts) = true;
                        LOG_VERIFIER_DEBUG_CONFIG_OPTION_IS_ACTIVE_INFO(s.name, l);
                    }
                }
            }
        }
        return true;
    };

    config::RegisterConfigHandler(dcfg, "config.debug.options.verifier", CONFIG_DEBUG_OPTIONS_VERIFIER);
}

}  // namespace panda::verifier::debug
