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

#ifndef FUNCTION_AGENT_PLUGIN_INFO_H
#define FUNCTION_AGENT_PLUGIN_INFO_H
#include <string>
#include <unordered_map>
#include <vector>

namespace functionsystem::function_agent {
struct PluginInfo {
    std::string id;
    std::string address;  // 远程地址如: "localhost:50051"
    bool enabled = false;
    bool critical = false;                                      // 启动失败，是否影响agent启动
    std::string type;                                           // "local",  "grpc"
    long callTimeout = 60000;                                   // 调用超时时间，单位ms
    std::string logPath = "/home/sn/log/";                                        // 插件日志路径
    std::unordered_map<std::string, std::string> extra_params;  // 额外配置的JSON，如启动路径
};

using PluginGroup = std::vector<PluginInfo>;

/**
 * {"plugin_groups":{"virtual_env_plugins":[{"id":"venvPlugin",
 * "address":"127.0.0.1:50051","enabled":true,"extra_params":{}},
 * {"id":"uvPlugin","address":"127.0.0.1:50052","enabled":false,"extra_params":{}}],
 * "deploy_plugins":[{"id":"s3_deployer_plugin","address":"127.0.0.1:60001","enabled":true,
 * "extra_params":{"artifact_bucket":"my-deploy-bucket","region":"us-east-1"}}]}}
 */
struct PluginsConfigRecords {
    std::unordered_map<std::string, PluginGroup> pluginGroups;
};

// client type
const std::string TYPE_GRPC = "grpc";

const std::string PLUGIN_GROUPS = "plugin_groups";
const std::string GROUP_NAME_VIRTUAL_ENV_PLUGINS = "virtual_env_plugins";

const std::string PLUGIN_ID = "id";
const std::string PLUGIN_ADDRESS = "address";
const std::string PLUGIN_ENABLED = "enabled";
const std::string PLUGIN_CRITICAL = "critical";
const std::string PLUGIN_CALL_TIMEOUT = "call_timeout";
const std::string PLUGIN_TYPE = "type";
const std::string PLUGIN_LOG_PATH = "logPath";
const std::string PLUGIN_INIT_PARAMS = "init_params";

// init_params
const std::string BINARY_PATH = "binary_path";
const std::string VIRTUAL_ENV_IDLE_TIME = "virtual_env_idle_time";

// virtual env plugin method constants
const std::string METHOD_CLEAR = "Clear";
const std::string METHOD_PREPARE = "Prepare";
const std::string METHOD_HEALTHCHECK = "HealthCheck";
const std::string METHOD_INCREASE_REF = "IncreaseRef";
const std::string METHOD_DECREASE_REF = "DecreaseRef";

// common constants
const std::string CONSTANT_RUNTIME_ID = "runtimeID";
const std::string CONSTANT_VIRTUALENV_PLUGIN_MGR = "VirtualEnvPluginMgr";
const std::string CONSTANT_VIRTUALENV_EXISTS = "VIRTUALENV_EXISTS";
}  // namespace functionsystem::function_agent
#endif  // FUNCTION_AGENT_PLUGIN_INFO_H
