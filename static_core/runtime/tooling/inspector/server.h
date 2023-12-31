/**
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef PANDA_TOOLING_INSPECTOR_SERVER_H
#define PANDA_TOOLING_INSPECTOR_SERVER_H

#include "event_loop.h"

#include <functional>

namespace panda {
class JsonObject;
class JsonObjectBuilder;
}  // namespace panda

namespace panda::tooling::inspector {
class Server : public virtual EventLoop {  // NOLINT(fuchsia-virtual-inheritance)
public:
    virtual void OnValidate(std::function<void()> &&handler) = 0;
    virtual void OnOpen(std::function<void()> &&handler) = 0;
    virtual void OnFail(std::function<void()> &&handler) = 0;

    virtual void Call(const std::string &session_id, const char *method,
                      std::function<void(JsonObjectBuilder &)> &&params) = 0;

    void Call(const char *method, std::function<void(JsonObjectBuilder &)> &&params)
    {
        Call({}, method, std::move(params));
    }

    void Call(const std::string &session_id, const char *method)
    {
        Call(session_id, method, [](JsonObjectBuilder & /* builder */) {});
    }

    void Call(const char *method)
    {
        Call({}, method, [](JsonObjectBuilder & /* builder */) {});
    }

    virtual void OnCall(const char *method, std::function<void(const std::string &session_id, JsonObjectBuilder &result,
                                                               const JsonObject &params)> &&handler) = 0;
};
}  // namespace panda::tooling::inspector

#endif  // PANDA_TOOLING_INSPECTOR_SERVER_H
