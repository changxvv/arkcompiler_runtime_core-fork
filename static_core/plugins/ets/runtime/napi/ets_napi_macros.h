/**
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License"
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

#ifndef PANDA_PLUGINS_ETS_RUNTIME_NAPI_ETS_NAPI_MACROS_H_
#define PANDA_PLUGINS_ETS_RUNTIME_NAPI_ETS_NAPI_MACROS_H_

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define ETS_NAPI_ABORT_HOOK()                      \
    do {                                           \
        panda::ets::PandaEtsVM::Abort("ETS_NAPI"); \
    } while (false)

#define ETS_NAPI_ABORT_IF(pred)    \
    do {                           \
        if (pred) {                \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_NULL(p)  \
    do {                           \
        if ((p) == nullptr) {      \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_LZ(p)    \
    do {                           \
        if ((p) < 0) {             \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_NE(a, b) \
    do {                           \
        if ((a) != (b)) {          \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_LT(a, b) \
    do {                           \
        if ((a) < (b)) {           \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_LE(a, b) \
    do {                           \
        if ((a) <= (b)) {          \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_GT(a, b) \
    do {                           \
        if ((a) > (b)) {           \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_ABORT_IF_GE(a, b) \
    do {                           \
        if ((a) >= (b)) {          \
            ETS_NAPI_ABORT_HOOK(); \
        }                          \
    } while (false)

#define ETS_NAPI_RETURN_IF_NULL(p) \
    do {                           \
        if ((p) == nullptr) {      \
            return;                \
        }                          \
    } while (false)

#define ETS_NAPI_RETURN_NULL_IF_NULL(p) \
    do {                                \
        if ((p) == nullptr) {           \
            return nullptr;             \
        }                               \
    } while (false)

#define ETS_NAPI_RETURN_NULL_IF_LE(a, b) \
    do {                                 \
        if ((a) <= (b)) {                \
            return nullptr;              \
        }                                \
    } while (false)

#define ETS_NAPI_RETURN_FALSE_IF_NULL(a, b) \
    do {                                    \
        if ((p) == nullptr) {               \
            return ETS_FALSE;               \
        }                                   \
    } while (false)

// NOLINTEND(cppcoreguidelines-macro-usage)

#endif  // PANDA_PLUGINS_ETS_RUNTIME_NAPI_ETS_NAPI_MACROS_H_
