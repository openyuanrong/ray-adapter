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

#ifndef FUNCTION_AGENT_PLUGIN_MGR_H
#define FUNCTION_AGENT_PLUGIN_MGR_H
#include <string>

#include "async/async.hpp"
#include "common/proto/pb/posix/agent_plugin.pb.h"
#include "common/status/status.h"
#include "function_agent/plugin/plugin_config.h"

namespace functionsystem::function_agent {
class PluginMgr {
public:
    virtual ~PluginMgr() = default;
    virtual litebus::Future<Status> LoadPlugin(const PluginGroup &plugins) = 0;

    virtual litebus::Future<Status> Initialize(const PluginGroup &plugins) = 0;

    virtual void UnloadPlugin(const std::string &pluginID) = 0;

    virtual litebus::Future<Status> CallPlugin(const std::string &pluginID,
                                               std::shared_ptr<agent_plugin::PluginRequest> request,
                                               std::shared_ptr<agent_plugin::PluginResponse> response) = 0;

    virtual litebus::Future<std::string> GetPluginID(std::shared_ptr<agent_plugin::PluginRequest> request) = 0;

    virtual litebus::Future<bool> RecoverCache(const std::string &message) = 0;
};
}  // namespace functionsystem::function_agent

#endif  // FUNCTION_AGENT_PLUGIN_MGR_H
