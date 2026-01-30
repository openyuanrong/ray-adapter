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

#include "remote_plugin_client.h"

#include <grpcpp/grpcpp.h>

#include "function_agent/plugin/plugin_config.h"

namespace functionsystem::function_agent {
inline bool IsLocalAddress(const std::string &address)
{
    return address.find("localhost") != std::string::npos ||
           address.find("127.0.0.1") != std::string::npos ||
           address.find("[::1]") != std::string::npos;
}

inline bool IsUnixSocket(const std::string &address)
{
    return address.find("unix://") == 0;
}

RemotePluginClient::RemotePluginClient(const std::string &address, const long timeout)
{
    bool useInsecureChannel = IsLocalAddress(address) || IsUnixSocket(address);
    if (useInsecureChannel) {
        channel_ = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    } else {
        channel_ = grpc::CreateChannel(address, grpc::SslCredentials(grpc::SslCredentialsOptions()));
    }
    stub_ = agent_plugin::PluginService::NewStub(channel_);
    this->callTimeout_ = timeout;
    YRLOG_INFO("init remote plugin client, address: {}, useInsecureChannel {}", address, useInsecureChannel);
}

Status RemotePluginClient::Call(const agent_plugin::PluginRequest &request, agent_plugin::PluginResponse &response)
{
    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(callTimeout_);
    if (request.method() == METHOD_HEALTHCHECK) {
        // 健康检查预期很快返回
        deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(healthCheckTimeout_);
    }
    context.set_deadline(deadline);
    auto grpcStatus = stub_->Call(&context, request, &response);
    if (!grpcStatus.ok()) {
        YRLOG_ERROR("{}|{}|failed to call remote plugin client, error code: {}, error message: {}, error details: {}",
                    request.traceid(), request.requestid(), static_cast<int>(grpcStatus.error_code()),
                    grpcStatus.error_message(), grpcStatus.error_details());
        if (grpcStatus.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
            return Status(StatusCode::REQUEST_TIME_OUT,
                          "call remote plugin client time out after " + std::to_string(callTimeout_));
        }
        return Status(StatusCode::FAILED, "failed to call remote plugin client");
    }
    if (response.code() != 0) {
        YRLOG_ERROR("{}|{}|remote plugin client response is not ok, error code: {}, error message: {}",
                    request.traceid(), request.requestid(), response.code(), response.message());
        return Status(StatusCode::FAILED, "remote plugin client response is not ok, " + response.message());
    }
    return Status::OK();
}

void RemotePluginClient::Close()
{
    if (stub_) {
        stub_.reset();
        YRLOG_INFO("stub_ is reset");
    } else {
        YRLOG_WARN("stub_ is null");
    }

    if (channel_) {
        channel_.reset();
        YRLOG_INFO("channel_ is reset");
    } else {
        YRLOG_WARN("channel_ is null");
    }
}
}  // namespace functionsystem::function_agent
