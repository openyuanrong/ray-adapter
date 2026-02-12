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
#include "function_agent/plugin/process_util.h"

#include <sys/wait.h>

#include "common/utils/files.h"
#include "utils/os_utils.hpp"

namespace functionsystem::function_agent {
constexpr uint32_t KILL_PROCESS_TIMEOUT_SECONDS = 5;  // default 5s
std::shared_ptr<litebus::Exec> CreatePluginProcess(const PluginInfo &pluginInfo)
{
    const auto binaryPathIt = pluginInfo.extra_params.find(BINARY_PATH);
    if (binaryPathIt == pluginInfo.extra_params.end()) {
        return nullptr;
    }
    std::string cmd = binaryPathIt->second;

    litebus::ExecIO stdOut = litebus::ExecIO::CreatePipeIO();

    auto outFile = litebus::os::Join(pluginInfo.logPath, pluginInfo.id + "-process.log");
    if (!litebus::os::ExistPath(outFile) && TouchFile(outFile) != 0) {
        YRLOG_WARN("create plugin process out log file {} failed", outFile);
    } else {
        stdOut = litebus::ExecIO::CreateFileIO(outFile);
    }

    return litebus::Exec::CreateExec(cmd, litebus::None(), litebus::ExecIO::CreatePipeIO(), stdOut, stdOut, {}, {},
                                     false);
}

void KillPluginProcess(const pid_t pid, const std::string &pluginID)
{
    // Send initial SIGINT
    (void)kill(pid, SIGTERM);

    litebus::TimerTools::AddTimer(
        KILL_PROCESS_TIMEOUT_SECONDS * litebus::SECTOMILLI, "KillPluginProcess", [pluginID, pid]() {
            if (kill(pid, 0) != 0) {
                YRLOG_INFO("SIGINT killed plugin {} process {}", pluginID, std::abs(pid));
                return;
            }

            if (kill(pid, SIGKILL) != 0) {
                YRLOG_ERROR("kill plugin {} process {} failed, errno({})", pluginID, std::abs(pid), errno);
            }
            YRLOG_INFO("SIGKILL killed plugin {} process {}", pluginID, std::abs(pid));
        });
}
}  // namespace functionsystem::function_agent
