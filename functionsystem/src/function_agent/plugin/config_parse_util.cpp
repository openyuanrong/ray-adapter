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

#include "config_parse_util.h"

#include <nlohmann/json.hpp>

#include "common/logs/logging.h"

namespace functionsystem::function_agent {
using json = nlohmann::json;

PluginInfo ParsePlugin(const json &config)
{
    PluginInfo info{};

    // 必填字段
    info.id = config.at(PLUGIN_ID).get<std::string>();
    info.address = config.at(PLUGIN_ADDRESS).get<std::string>();

    // 可选字段：type, 默认为空（=local)
    if (config.contains(PLUGIN_TYPE)) {
        info.type = config[PLUGIN_TYPE].get<std::string>();
    }

    // 可选字段：enabled，默认 FALSE
    if (config.contains(PLUGIN_ENABLED)) {
        info.enabled = config[PLUGIN_ENABLED].get<bool>();
    }

    // 可选字段：critical，默认 FALSE
    if (config.contains(PLUGIN_CRITICAL)) {
        info.critical = config[PLUGIN_CRITICAL].get<bool>();
    }

    // 可选字段：callTimeout，默认 60s
    if (config.contains(PLUGIN_CALL_TIMEOUT)) {
        info.callTimeout = config[PLUGIN_CALL_TIMEOUT].get<long>();
    }

    // 可选字段：logPath，默认 /home/sn/log
    if (config.contains(PLUGIN_LOG_PATH)) {
        info.logPath = config[PLUGIN_LOG_PATH].get<std::string>();
    }

    // 可选字段：init_params，必须是 string -> string
    if (config.contains(PLUGIN_INIT_PARAMS) && config[PLUGIN_INIT_PARAMS].is_object()) {
        for (auto it = config[PLUGIN_INIT_PARAMS].begin(); it != config[PLUGIN_INIT_PARAMS].end(); ++it) {
            // 强制转换为 string（即使 JSON 中是数字/bool，也转成字符串）
            info.extra_params[it.key()] = it.value().get<std::string>();
        }
    }

    return info;
}

PluginsConfigRecords ParsePluginsConfig(const std::string &input)
{
    PluginsConfigRecords records;
    try {
        nlohmann::json configJson = nlohmann::json::parse(input);
        if (!configJson.contains(PLUGIN_GROUPS) || !configJson[PLUGIN_GROUPS].is_object()) {
            YRLOG_WARN("{} object is missing or invalid", PLUGIN_GROUPS);
            return records;
        }
        const auto &groups = configJson[PLUGIN_GROUPS];
        for (auto groupIt = groups.begin(); groupIt != groups.end(); ++groupIt) {
            const std::string groupName = groupIt.key();
            const auto &pluginArray = groupIt.value();

            if (!pluginArray.is_array()) {
                YRLOG_WARN("Plugin group {} must be an array", groupName);
                continue;
            }

            PluginGroup group;
            for (const auto &pluginJson : pluginArray) {
                if (!pluginJson.is_object()) {
                    YRLOG_WARN("Plugin {} is invalid, input {}", groupName, input);
                    continue;
                }
                group.push_back(ParsePlugin(pluginJson));
            }
            records.pluginGroups[groupName] = std::move(group);
        }
    } catch (std::exception &e) {
        YRLOG_WARN("failed to parse ({}) to json, error: {}", input, e.what());
    }
    return records;
}

PluginGroup GetPluginConfigOfGroup(const PluginsConfigRecords &records, const std::string &groupName)
{
    auto iter = records.pluginGroups.find(groupName);
    if (iter == records.pluginGroups.end()) {
        return PluginGroup{};
    }
    return iter->second;
}
}  // namespace functionsystem::function_agent
