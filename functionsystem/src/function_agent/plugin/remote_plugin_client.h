/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
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

#ifndef FUNCTION_AGENT_REMOTE_PLUGIN_CLIENT_H
#define FUNCTION_AGENT_REMOTE_PLUGIN_CLIENT_H

#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>

#include "function_agent/plugin/plugin_client.h"
#include "common/status/status.h"

namespace functionsystem::function_agent {
    class RemotePluginClient : public PluginClient {
    public:
        explicit RemotePluginClient(const std::string &address, const long timeout = 60000);

        RemotePluginClient(const RemotePluginClient &) = delete;

        RemotePluginClient &operator=(const RemotePluginClient &) = delete;

        ~RemotePluginClient() = default;

        Status Call(const agent_plugin::PluginRequest &request, agent_plugin::PluginResponse &response) override;

        void Close() override;

    private:
        std::unique_ptr<agent_plugin::PluginService::Stub> stub_;
        std::shared_ptr<grpc::Channel> channel_;
        long callTimeout_ = 60000; // ms
        long healthCheckTimeout_ = 5000; // ms
    };
}

#endif // FUNCTION_AGENT_REMOTE_PLUGIN_CLIENT_H
