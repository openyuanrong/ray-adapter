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
#ifndef FUNCTION_AGENT_PROCESS_UTIL_H
#define FUNCTION_AGENT_PROCESS_UTIL_H
#include <memory>

#include "common/logs/logging.h"
#include "exec/exec.hpp"
#include "function_agent/plugin/plugin_config.h"

namespace functionsystem::function_agent {
std::shared_ptr<litebus::Exec> CreatePluginProcess(const PluginInfo &pluginInfo);

void KillPluginProcess(const pid_t pid, const std::string &pluginID);
}  // namespace functionsystem::function_agent
#endif
