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
#include <unordered_map>

#include "function_agent/plugin/plugin_client.h"
#include "function_agent/plugin/plugin_config.h"
#include "function_agent/plugin/plugin_mgr.h"
#include "exec/exec.hpp"
#include "timer/timer.hpp"

namespace functionsystem::function_agent {
    constexpr uint32_t DEFAULT_RECYCLE_UNUSED_ENVS_INTERVAL = 15;
    constexpr uint32_t DEFAULT_HEALTH_CHECK_INTERVAL = 15;

    struct EnvInfo {
        std::unordered_set<std::string> runtimeIds;
        std::string pluginID;
        std::string envName;
        std::string envType;
        uint64_t lastRemoveTimestamp{0};
    };

    class VirtualEnvPluginMgr final : public PluginMgr {
    public:
        ~VirtualEnvPluginMgr() override
        {
            StopHealthMonitor();
            StopRefRecycleMonitor();
            pluginConfigs_.clear();
            envReferInfos_.clear();
        }

        Status CallPlugin(const std::string &pluginID, agent_plugin::PluginRequest &request,
                          agent_plugin::PluginResponse &response) override;

        Status LoadPlugin(const PluginGroup &plugins) override;

        Status Initialize(const PluginGroup &plugins) override;

        bool UnloadPlugin(const std::string &pluginID) override;

        bool IsHealthy(const std::string &pluginID) override;

        bool RecoverCache(const std::string &message) override;

        const std::string GetPluginID(const agent_plugin::PluginRequest &request) override;

    private:
        void SetVirtualEnvIdleTime(const PluginInfo &pluginInfo);

        Status LoadPlugin(const PluginInfo &pluginInfo);

        Status InitPlugin(const PluginInfo &pluginInfo);

        void HealthCheck();

        void StartHealthMonitor();

        void StopHealthMonitor();

        void StartRefRecycleMonitor();

        void StopRefRecycleMonitor();

        void RecycleUnusedEnvs();

        void AddEnvReferInfos(const std::string &envName, const std::string &envType, const std::string &pluginID,
                              const std::string &runtimeID);

        void RmEnvReferInfos(const std::string &runtimeID);

        void GetEnvNameNeed2Clear(std::map<std::string, EnvInfo> &pluginID2EnvInfo);

        void DoClearVirtualEnv(std::unique_lock<std::mutex> &lock);

        Status CallVirtualEnvMgrMethod(const agent_plugin::PluginRequest &request);

        Status CallSpecificPlugin(const std::string &pluginID, agent_plugin::PluginRequest &request,
                                  agent_plugin::PluginResponse &response);

        std::shared_ptr<PluginClient> GetPlugin(const std::string &pluginID);

        // pluginID:pluginClient
        std::unordered_map<std::string, std::shared_ptr<PluginClient> > pluginClients_;
        std::unordered_map<std::string, PluginInfo> pluginConfigs_;
        std::map<std::string, std::shared_ptr<litebus::Exec> > pluginID2ExecPtr_;

        // key:envName_envType，value: envInfo
        std::unordered_map<std::string, EnvInfo> envReferInfos_;
        litebus::Timer recycleUnusedEnvsTimer_;
        const std::chrono::seconds recycleUnusedEnvsInterval_{DEFAULT_RECYCLE_UNUSED_ENVS_INTERVAL};
        std::mutex envRefMutex_;
        int virtualEnvIdleTime_ = -1;
        std::thread refRecycleThread_;
        std::atomic<bool> stopRefRecycle_{false};

        // health check
        std::mutex healthyCheckMutex_;
        std::thread healthMonitorThread_;
        std::chrono::seconds healthCheckInterval_{DEFAULT_HEALTH_CHECK_INTERVAL};
        std::map<std::string, bool> healthyCheckResults_;
        std::atomic<bool> stopHealthCheck_{false};

        const std::chrono::milliseconds checkInterval_{1000};
    };
} // namespace functionsystem::function_agent

#endif  // FUNCTION_AGENT_VIRTUALENV_PLUGIN_MGR_H
