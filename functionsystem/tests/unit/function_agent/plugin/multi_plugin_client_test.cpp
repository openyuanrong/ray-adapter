/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "function_agent/plugin/multi_plugin_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/constants/constants.h"
#include "common/proto/pb/posix/agent_plugin.pb.h"
#include "function_agent/plugin/plugin_mgr.h"
#include "function_agent/plugin/process_util.h"
#include "function_agent/plugin/virtualenv_plugin_mgr_actor.h"

namespace functionsystem::test {
class MockPluginManager : public function_agent::PluginMgr, public litebus::ActorBase {
public:
    explicit MockPluginManager(const std::string &name = "mockManagerActor") : ActorBase(name)
    {
    }
    ~MockPluginManager()
    {
        litebus::Terminate(GetAID());
        litebus::Await(GetAID());
    }
    MOCK_METHOD(litebus::Future<Status>, LoadPlugin, (const function_agent::PluginGroup &plugins), (override));
    MOCK_METHOD(litebus::Future<Status>, Initialize, (const function_agent::PluginGroup &plugins), (override));
    MOCK_METHOD(void, UnloadPlugin, (const std::string &pluginID), (override));
    MOCK_METHOD(litebus::Future<Status>, CallPlugin,
                (const std::string &pluginID, std::shared_ptr<agent_plugin::PluginRequest> request,
                 std::shared_ptr<agent_plugin::PluginResponse> response),
                (override));
    MOCK_METHOD(litebus::Future<bool>, RecoverCache, (const std::string &message), (override));
    MOCK_METHOD(litebus::Future<std::string>, GetPluginID, (std::shared_ptr<agent_plugin::PluginRequest> request),
                (override));
};

class MultiPluginClientTest : public testing::Test {};

TEST_F(MultiPluginClientTest, ParsePluginsConfigFailed)
{
    std::string config = "invalid json";

    function_agent::MultiPluginClient client;
    Status status = client.RegisterPlugin(config).Get();

    // 初始化插件配置解析失敗不影响启动
    EXPECT_EQ(status.StatusCode(), StatusCode::SUCCESS);
    EXPECT_TRUE(client.pluginMgrs_.empty());
}

TEST_F(MultiPluginClientTest, VirtualEnvPluginManagerExists)
{
    std::string config = R"({
        "plugin_groups": {
            "group1": [
                {"id": "plugin1", "address": "localhost:8080"}
            ]
        }
    })";

    function_agent::MultiPluginClient client;
    client.pluginGroupMgrs_.emplace(function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS, client.virtualEnvMgrActor_);

    Status status = client.RegisterPlugin(config).Get();

    EXPECT_EQ(status.StatusCode(), StatusCode::SUCCESS);
    EXPECT_FALSE(client.pluginGroupMgrs_.empty());
    // will not add another VirtualEnvPluginMgr
    ASSERT_EQ(client.pluginGroupMgrs_.size(), 1);
}

// Test for PrepareEnv
TEST_F(MultiPluginClientTest, VirtualEnvPluginManagerNotFound)
{
    auto request = std::make_shared<messages::DeployInstanceRequest>();
    request->mutable_createoptions()->insert({ VIRTUALENV_KIND, "venv" });
    request->set_traceid("trace1");

    function_agent::MultiPluginClient client;
    MockPluginManager mockPluginManager;
    // 校验无virtualEnvName时不进入virtualenv_plugin_mgr的逻辑
    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(0);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_.erase(function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS);
    Status status = client.PrepareEnv(request).Get();
    EXPECT_EQ(status.StatusCode(), StatusCode::FAILED);
    EXPECT_EQ(status.GetMessage(), "[virtual env plugin manager not found]");
}

TEST_F(MultiPluginClientTest, VirtualEnvNameNotFound)
{
    auto request = std::make_shared<messages::DeployInstanceRequest>();
    request->set_traceid("trace1");

    function_agent::MultiPluginClient client;
    Status status = client.PrepareEnv(request).Get();

    EXPECT_EQ(status.StatusCode(), StatusCode::SUCCESS);
}

TEST_F(MultiPluginClientTest, GetPluginIDFailed)
{
    auto request = std::make_shared<messages::DeployInstanceRequest>();
    request->mutable_createoptions()->insert({ VIRTUALENV_KIND, "venv2" });
    request->set_traceid("trace1");

    function_agent::MultiPluginClient client;
    client.pluginGroupMgrs_.emplace(function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS,
                                        client.virtualEnvMgrActor_);

    Status status = client.PrepareEnv(request).Get();
    EXPECT_EQ(status.StatusCode(), StatusCode::FAILED);
    EXPECT_EQ(status.GetMessage(), "[get plugin ID failed]");
}

TEST_F(MultiPluginClientTest, RecoverPluginCache_EmptyPluginGroup)
{
    function_agent::MultiPluginClient client;

    // 测试空插件组
    bool result = client.RecoverPluginCache("test_message").Get();

    EXPECT_TRUE(result);  // 空组应返回true
}

TEST_F(MultiPluginClientTest, IncreaseVirtualEnvRef_NotContainVirtualName)
{
    function_agent::MultiPluginClient client;

    MockPluginManager mockPluginManager;
    // 校验无virtualEnvName时不进入virtualenv_plugin_mgr的逻辑
    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(0);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_.emplace(function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS,
                                    std::static_pointer_cast<function_agent::PluginMgr>(pluginManager));
    auto request = std::make_shared<messages::DeployInstanceRequest>();
    request->set_traceid("trace1");

    client.IncreaseEnvRef(request, "runtimeID");
}

TEST_F(MultiPluginClientTest, IncreaseVirtualEnvRef_NotRegisterVirtualEnvPluginMgr)
{
    function_agent::MultiPluginClient client;

    MockPluginManager mockPluginManager;
    // 校验VirtualEnvPluginMgr未注册时不进入virtualenv_plugin_mgr的逻辑
    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(0);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_.emplace("group1", std::static_pointer_cast<function_agent::PluginMgr>(pluginManager));
    auto request = std::make_shared<messages::DeployInstanceRequest>();
    request->mutable_createoptions()->insert({ VIRTUALENV_NAME, "env1" });
    request->set_traceid("trace1");

    client.IncreaseEnvRef(request, "runtimeID");
}

TEST_F(MultiPluginClientTest, IncreaseVirtualEnvRef_InvokePlugin)
{
    function_agent::MultiPluginClient client;

    MockPluginManager mockPluginManager;

    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(1);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_[function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS] =
                                    std::static_pointer_cast<function_agent::PluginMgr>(pluginManager);
    auto request = std::make_shared<messages::DeployInstanceRequest>();
    request->mutable_createoptions()->insert({ VIRTUALENV_NAME, "env1" });
    request->mutable_createoptions()->insert({ VIRTUALENV_KIND, "venv" });
    request->set_traceid("trace1");

    client.IncreaseEnvRef(request, "runtimeID");
}

TEST_F(MultiPluginClientTest, DecreaseVirtualEnvRef_NotContainVirtualName)
{
    function_agent::MultiPluginClient client;

    MockPluginManager mockPluginManager;
    // 校验无virtualEnvName时不进入virtualenv_plugin_mgr的逻辑
    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(0);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_[function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS] =
                                    std::static_pointer_cast<function_agent::PluginMgr>(pluginManager);
    messages::RuntimeInstanceInfo runtimeInstanceInfo;
    runtimeInstanceInfo.set_traceid("traceID");
    runtimeInstanceInfo.set_requestid("requestID");
    client.DecreaseEnvRef("runtimeID", runtimeInstanceInfo);
}

TEST_F(MultiPluginClientTest, DecreaseVirtualEnvRef_NotRegisterVirtualEnvPluginMgr)
{
    function_agent::MultiPluginClient client;

    MockPluginManager mockPluginManager;
    // 校验VirtualEnvPluginMgr未注册时不进入virtualenv_plugin_mgr的逻辑
    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(0);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_.emplace("group1", std::static_pointer_cast<function_agent::PluginMgr>(pluginManager));
    client.pluginGroupMgrs_.erase(function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS);
    messages::RuntimeInstanceInfo runtimeInstanceInfo;
    runtimeInstanceInfo.set_traceid("traceID");
    runtimeInstanceInfo.set_requestid("requestID");
    runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(VIRTUALENV_NAME, "env1");
    client.DecreaseEnvRef("runtimeID", runtimeInstanceInfo);
}

TEST_F(MultiPluginClientTest, DecreaseVirtualEnvRef_InvokePlugin)
{
    function_agent::MultiPluginClient client;

    MockPluginManager mockPluginManager;

    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_)).Times(1);

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginGroupMgrs_[function_agent::GROUP_NAME_VIRTUAL_ENV_PLUGINS] =
                                    std::static_pointer_cast<function_agent::PluginMgr>(pluginManager);
    messages::RuntimeInstanceInfo runtimeInstanceInfo;
    runtimeInstanceInfo.set_traceid("traceID");
    runtimeInstanceInfo.set_requestid("requestID");
    runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(VIRTUALENV_NAME, "env1");
    runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(VIRTUALENV_KIND, "venv");
    client.DecreaseEnvRef("runtimeID", runtimeInstanceInfo);
}

TEST_F(MultiPluginClientTest, Call_NotFoundPluginMgr)
{
    function_agent::MultiPluginClient client;
    auto testRequest = std::make_shared<agent_plugin::PluginRequest>();
    auto testResponse = std::make_shared<agent_plugin::PluginResponse>();
    Status status = client.Call("pluginID_not_found", testRequest, testResponse).Get();
    EXPECT_EQ(status.StatusCode(), StatusCode::FAILED);
}

TEST_F(MultiPluginClientTest, Call_Success)
{
    function_agent::MultiPluginClient client;
    MockPluginManager mockPluginManager;

    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(litebus::Future(Status(SUCCESS))));

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});

    litebus::Spawn(pluginManager);
    client.pluginMgrs_.emplace("pluginID", std::static_pointer_cast<function_agent::PluginMgr>(pluginManager));

    auto testRequest = std::make_shared<agent_plugin::PluginRequest>();
    auto testResponse = std::make_shared<agent_plugin::PluginResponse>();
    Status status = client.Call("pluginID", testRequest, testResponse).Get();
    EXPECT_EQ(status.StatusCode(), StatusCode::SUCCESS);
}

TEST_F(MultiPluginClientTest, Call_Failed)
{
    function_agent::MultiPluginClient client;
    MockPluginManager mockPluginManager;
    auto status = Status(StatusCode::FAILED, "test failed");
    EXPECT_CALL(mockPluginManager, CallPlugin(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(litebus::Future(status)));

    auto pluginManager = std::shared_ptr<MockPluginManager>(&mockPluginManager, [](auto) {});
    litebus::Spawn(pluginManager);

    client.pluginMgrs_.emplace("pluginID", std::static_pointer_cast<function_agent::PluginMgr>(pluginManager));

    auto testRequest = std::make_shared<agent_plugin::PluginRequest>();
    auto testResponse = std::make_shared<agent_plugin::PluginResponse>();
    status = client.Call("pluginID", testRequest, testResponse).Get();
    EXPECT_EQ(status.StatusCode(), StatusCode::FAILED);
    EXPECT_EQ(status.GetMessage(), "[test failed]");
}
}  // namespace functionsystem::test
