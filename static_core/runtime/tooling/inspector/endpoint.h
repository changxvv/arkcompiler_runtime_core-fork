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

#ifndef PANDA_TOOLING_INSPECTOR_ENDPOINT_H
#define PANDA_TOOLING_INSPECTOR_ENDPOINT_H

#include "macros.h"
#include "utils/json_builder.h"
#include "utils/logger.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "websocketpp/common/connection_hdl.hpp"
#include "websocketpp/frame.hpp"
#pragma GCC diagnostic pop

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace panda {
class JsonObject;
}  // namespace panda

namespace panda::tooling::inspector {
class EndpointBase {
public:
    using Id = double;

private:
    using MethodHandler =
        std::function<void(const std::string &session_id, std::optional<Id>, const JsonObject &params)>;
    using ResultHandler = std::function<void(const JsonObject &)>;

public:
    void OnCall(const char *method, MethodHandler &&handler);

protected:
    void HandleMessage(const std::string &message);

    void OnResult(Id id, ResultHandler &&handler)
    {
        result_handlers_[id] = std::move(handler);
    }

private:
    os::memory::Mutex method_handlers_mutex_;
    std::unordered_map<std::string, MethodHandler> method_handlers_ GUARDED_BY(method_handlers_mutex_);
    std::unordered_map<Id, ResultHandler> result_handlers_;
};

// JSON-RPC endpoint handling the Inspector protocol.
template <typename WsEndpoint>
class Endpoint : public EndpointBase {
public:
    Endpoint()
    {
        endpoint_.set_message_handler([this](auto /* hdl */, auto message) { HandleMessage(message->get_payload()); });
    }

protected:
    void Call(
        const std::string &session_id, std::optional<Id> id, const char *method,
        std::function<void(JsonObjectBuilder &)> &&params = [](JsonObjectBuilder & /* builder */) {})
    {
        Send([&session_id, id, method, &params](JsonObjectBuilder &call) {
            if (id) {
                call.AddProperty("id", *id);
            }

            call.AddProperty("method", method);
            call.AddProperty("params", std::move(params));

            if (!session_id.empty()) {
                call.AddProperty("sessionId", session_id);
            }
        });
    }

    template <typename Result>
    void Reply(const std::string &session_id, Id id, Result &&result)
    {
        Send([&session_id, id, &result](JsonObjectBuilder &reply) {
            reply.AddProperty("id", id);
            reply.AddProperty("result", std::forward<Result>(result));

            if (!session_id.empty()) {
                reply.AddProperty("sessionId", session_id);
            }
        });
    }

    typename WsEndpoint::connection_ptr GetPinnedConnection()
    {
        return connection_;
    }

    bool Pin(const websocketpp::connection_hdl &hdl)
    {
        typename WsEndpoint::connection_ptr expected;
        return std::atomic_compare_exchange_strong(&connection_, &expected, endpoint_.get_con_from_hdl(hdl));
    }

    void Unpin(const websocketpp::connection_hdl &hdl)
    {
        auto old = endpoint_.get_con_from_hdl(hdl);
        auto expected = old;
        std::atomic_compare_exchange_strong(&connection_, &expected, {});
    }

    WsEndpoint endpoint_;  // NOLINT(misc-non-private-member-variables-in-classes)

private:
    template <typename BuildFunction>
    void Send(BuildFunction &&build)
    {
        JsonObjectBuilder builder;
        build(builder);
        auto message = std::move(builder).Build();
        LOG(DEBUG, DEBUGGER) << "Sending " << message;
        if (auto connection = GetPinnedConnection()) {
            connection->send(message, websocketpp::frame::opcode::text);
        }
    }

    typename WsEndpoint::connection_ptr connection_;
};
}  // namespace panda::tooling::inspector

#endif  // PANDA_TOOLING_INSPECTOR_ENDPOINT_H
