/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include <gtest/gtest.h>
#include <memory>

#include "function_agent/plugin/process_util.h"
#include "function_agent/plugin/plugin_config.h"

namespace functionsystem::test {
    namespace test_process_utils {
        pid_t mockProcess() {
            pid_t pid = fork();
            if (pid == 0) {
                // 子进程逻辑，这里可以是简单的无限循环
                while (true) {
                    sleep(1);
                }
            }
            return pid;
        }
    }
    class ProcessUtilTest : public testing::Test {
    };


    TEST_F(ProcessUtilTest, CreatePluginProcess_WithValidBinaryPath) {
        function_agent::PluginInfo pluginInfo;
        pluginInfo.extra_params[function_agent::BINARY_PATH] = "/path/to/fake/binary";

        auto exec = functionsystem::function_agent::CreatePluginProcess(pluginInfo);
        EXPECT_NE(exec, nullptr);

        // 检查是否正确处理了没有BINARY_PATH的情况
        pluginInfo.extra_params.erase(function_agent::BINARY_PATH);
        auto nullExec = functionsystem::function_agent::CreatePluginProcess(pluginInfo);
        EXPECT_EQ(nullExec, nullptr);
    }

    TEST_F(ProcessUtilTest, KillPluginProcessTest) {
        pid_t pid = test_process_utils::mockProcess();
        ASSERT_NE(pid, 0);

        functionsystem::function_agent::KillPluginProcess(pid, "pluginID");

        int status;
        for (int i = 0; i < 10; ++i) {
            pid_t ret = waitpid(pid, &status, WNOHANG);
            if (ret == pid) {
                // 成功回收
                EXPECT_TRUE(WIFSIGNALED(status)); // 应该是被信号杀死的
                EXPECT_TRUE(WTERMSIG(status) == SIGKILL || WTERMSIG(status) == SIGTERM);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        // 超时未退出
        FAIL() << "Plugin process (pid=" << pid << ") did not terminate after SIGKILL";
    }

}
