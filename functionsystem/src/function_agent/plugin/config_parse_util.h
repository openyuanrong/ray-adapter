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
#ifndef FUNCTION_AGENT_CONFIG_PARSE_UTIL_H
#define FUNCTION_AGENT_CONFIG_PARSE_UTIL_H

#include <nlohmann/json.hpp>

#include "plugin_config.h"

namespace functionsystem::function_agent {
using json = nlohmann::json;

// 单个plugin配置解析
PluginInfo ParsePlugin(const json &config);

// 整个json字符串解析,之后如果从文件里读，也可以复用
PluginsConfigRecords ParsePluginsConfig(const std::string &input);

// 获取groupName对应的插件配置
PluginGroup GetPluginConfigOfGroup(const PluginsConfigRecords &records, const std::string &groupName);
}  // namespace functionsystem::function_agent
#endif  // FUNCTION_AGENT_CONFIG_PARSE_UTIL_H;
