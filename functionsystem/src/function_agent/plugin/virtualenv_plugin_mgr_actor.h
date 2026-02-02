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

#ifndef FUNCTION_AGENT_VIRTUALENV_PLUGIN_MGR_H
#define FUNCTION_AGENT_VIRTUALENV_PLUGIN_MGR_H

#include <memory>

#include "actor/actor.hpp"
#include "async/async.hpp"
#include "exec/exec.hpp"
#include "function_agent/plugin/plugin_config.h"
#include "function_agent/plugin/plugin_mgr.h"
#include "function_agent/plugin/plugin_client.h"

namespace functionsystem::function_agent {
constexpr uint32_t DEFAULT_RECYCLE_UNUSED_ENVS_INTERVAL = 15000;
constexpr uint32_t DEFAULT_HEALTH_CHECK_INTERVAL = 15000;
struct EnvInfo {
    std::unordered_set<std::string> runtimeIds;
    std::string pluginID;
    std::string envName;
    std::string envType;
    uint64_t lastRemoveTimestamp{ 0 };
};
class VirtualEnvPluginMgrActor final : public PluginMgr, public litebus::ActorBase {
public:
  explicit VirtualEnvPluginMgrActor(const std::string &name);
    ~VirtualEnvPluginMgrActor() override;

    litebus::Future<Status> LoadPlugin(const PluginGroup &plugins) override;

    litebus::Future<Status> Initialize(const PluginGroup &plugins) override;

    void UnloadPlugin(const std::string &pluginID) override;

    litebus::Future<Status> CallPlugin(const std::string &pluginID,
                                       std::shared_ptr<agent_plugin::PluginRequest> request,
                                       std::shared_ptr<agent_plugin::PluginResponse> response) override;

    litebus::Future<std::string> GetPluginID(std::shared_ptr<agent_plugin::PluginRequest> request) override;

    litebus::Future<bool> RecoverCache(const std::string &message) override;
protected:
  void Init() override;

  void Finalize() override;

private:
    bool IsHealthy(const std::string &pluginID);
    Status InitPlugin(const PluginInfo &pluginInfo);
    Status LoadPlugin(const PluginInfo &pluginInfo);
    void SetVirtualEnvIdleTime(const PluginInfo &pluginInfo);

    void HealthCheck();

    void RecycleUnusedEnvs();

    void AddEnvReferInfos(const std::string &envName, const std::string &envType, const std::string &pluginID,
                          const std::string &runtimeID);

    void RmEnvReferInfos(const std::string &runtimeID);

    void GetEnvNameNeed2Clear(std::map<std::string, EnvInfo> &pluginID2EnvInfo);

    void DoClearVirtualEnv();

    litebus::Future<Status> CallVirtualEnvMgrMethod(std::shared_ptr<agent_plugin::PluginRequest> request);

    litebus::Future<Status> CallSpecificPlugin(const std::string &pluginID,
                                               std::shared_ptr<agent_plugin::PluginRequest> request,
                                               std::shared_ptr<agent_plugin::PluginResponse> response);

    std::shared_ptr<PluginClient> GetPlugin(const std::string &pluginID);
    litebus::Future<Status> CallPluginImpl(std::shared_ptr<agent_plugin::PluginRequest> request,
                                       std::shared_ptr<agent_plugin::PluginResponse> response,
                                       std::shared_ptr<PluginClient> pluginClient);

    // pluginID:pluginClient
    std::unordered_map<std::string, std::shared_ptr<PluginClient> > pluginClients_;
    std::unordered_map<std::string, PluginInfo> pluginConfigs_;
    std::map<std::string, std::shared_ptr<litebus::Exec> > pluginID2ExecPtr_;

    // key:envName_envType，value: envInfo
    std::unordered_map<std::string, EnvInfo> envReferInfos_;
    uint32_t envRefRecycleInterval_{ DEFAULT_RECYCLE_UNUSED_ENVS_INTERVAL };
    int virtualEnvIdleTime_ = -1;
    litebus::Timer recycleUnusedEnvsTimer_;

    // health check
    std::map<std::string, bool> healthyCheckResults_;
    uint32_t healthCheckInterval_{ DEFAULT_HEALTH_CHECK_INTERVAL };
    litebus::Timer healthCheckTimer_;
};
}  // namespace functionsystem::function_agent

#endif  // FUNCTION_AGENT_VIRTUALENV_PLUGIN_MGR_H
