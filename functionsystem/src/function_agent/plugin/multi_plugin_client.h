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

#ifndef FUNCTION_AGENT_MULTI_PLUGIN_CLIENT_H
#define FUNCTION_AGENT_MULTI_PLUGIN_CLIENT_H
#include <memory>
#include <string>
#include <unordered_map>

#include "common/proto/pb/posix/message.pb.h"
#include "common/status/status.h"
#include "function_agent/plugin/plugin_mgr.h"
#include "virtualenv_plugin_mgr_actor.h"

namespace functionsystem::function_agent {
class MultiPluginClient {
public:
    MultiPluginClient();
    ~MultiPluginClient();
    litebus::Future<Status> RegisterPlugin(const std::string &config);

    litebus::Future<bool> RecoverPluginCache(const std::string &message);

    litebus::Future<Status> PrepareEnv(const std::shared_ptr<messages::DeployInstanceRequest> &request);

    void IncreaseEnvRef(const std::shared_ptr<messages::DeployInstanceRequest> &request, const std::string &runtimeID);

    void DecreaseEnvRef(const std::string &runtimeID, const messages::RuntimeInstanceInfo &info);

    litebus::Future<Status> Call(const std::string &pluginID, std::shared_ptr<agent_plugin::PluginRequest> request,
                                 std::shared_ptr<agent_plugin::PluginResponse> response);

private:
    litebus::Future<Status> PrepareVirtualEnv(const std::shared_ptr<messages::DeployInstanceRequest> &request);

    void IncreaseVirtualEnvRef(const std::shared_ptr<messages::DeployInstanceRequest> &request,
                               const std::string &runtimeID);

    void DecreaseVirtualEnvRef(const std::string &runtimeID, const messages::RuntimeInstanceInfo &info);

    std::unordered_map<std::string, std::shared_ptr<PluginMgr> > pluginMgrs_;
    // groupName:PluginMgr
    std::unordered_map<std::string, std::shared_ptr<PluginMgr> > pluginGroupMgrs_;

    // requestID:venvName
    std::unordered_map<std::string, std::string> requestID2VenvName_;

    std::shared_ptr<VirtualEnvPluginMgrActor> virtualEnvMgrActor_;
};
}  // namespace functionsystem::function_agent

#endif  // FUNCTION_AGENT_MULTI_PLUGIN_CLIENT_H
