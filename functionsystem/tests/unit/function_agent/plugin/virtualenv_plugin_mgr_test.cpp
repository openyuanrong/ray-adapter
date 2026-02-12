/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "function_agent/plugin/virtualenv_plugin_mgr_actor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "function_agent/plugin/plugin_config.h"
#include "common/constants/constants.h"
#include "common/proto/pb/posix/message.pb.h"

namespace functionsystem::test {
    using namespace function_agent;

    class MockPluginClient : public PluginClient {
    public:
        MOCK_METHOD(Status, Call, (const agent_plugin::PluginRequest &request,
                        agent_plugin::PluginResponse &response), (override));
        MOCK_METHOD(void, Close, (), (override));
    };

    class VirtualEnvPluginMgrActorTest : public testing::Test {
        void SetUp() override {
            // 初始化测试数据
            PluginInfo normalPlugin{};
            normalPlugin.id = "normal_plugin";
            normalPlugin.enabled = true;

            PluginInfo criticalPlugin{};
            criticalPlugin.id = "critical_plugin";
            criticalPlugin.enabled = true;
            criticalPlugin.critical = true;
            criticalPlugin.extra_params = { { BINARY_PATH, "/xxx" } };

            PluginInfo notEnablePlugin{};
            notEnablePlugin.id = "not_enable_plugin";

            PluginInfo pluginInfo;
            pluginInfo.id = "pluginID";
            pluginInfo.enabled = true;
            pluginInfo.critical = true;
            pluginInfo.type = "grpc";

            normalPlugins_.push_back(normalPlugin);
            notEnablePlugins_.push_back(notEnablePlugin);
            criticalPlugins_.push_back(criticalPlugin);
            initSuccessPlugins_.push_back(pluginInfo);
        }

        PluginGroup normalPlugins_;
        PluginGroup criticalPlugins_;
        PluginGroup notEnablePlugins_;
        PluginGroup initSuccessPlugins_;
    };

    TEST_F(VirtualEnvPluginMgrActorTest, LoadPlugin_Success) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        // 测试正常插件加载
        Status result = pluginMgr.LoadPlugin(notEnablePlugins_).Get();
        EXPECT_FALSE(result.IsError());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, LoadPlugin_FailWithNonCriticalPlugin) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        // 测试非关键插件加载失败
        Status result = pluginMgr.LoadPlugin(normalPlugins_).Get();
        EXPECT_FALSE(result.IsError()); // 非关键插件失败不应影响整体
    }

    TEST_F(VirtualEnvPluginMgrActorTest, LoadPlugin_FailWithCriticalPlugin) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        Status result = pluginMgr.LoadPlugin(criticalPlugins_).Get();
        EXPECT_TRUE(result.IsError()); // 关键插件失败影响整体
    }

    TEST_F(VirtualEnvPluginMgrActorTest, UnloadPlugin_NotExist_And_Exist) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        pluginMgr.healthyCheckResults_["pluginID"] = true;
        pluginMgr.pluginClients_["pluginID"] = nullptr;

        EXPECT_EQ(pluginMgr.healthyCheckResults_.size(), 1);
        EXPECT_EQ(pluginMgr.pluginClients_.size(), 1);

        pluginMgr.UnloadPlugin("notExistPluginID");

        EXPECT_EQ(pluginMgr.healthyCheckResults_.size(), 1);
        EXPECT_EQ(pluginMgr.pluginClients_.size(), 1);

        pluginMgr.UnloadPlugin("pluginID");

        EXPECT_EQ(pluginMgr.healthyCheckResults_.size(), 0);
        EXPECT_EQ(pluginMgr.pluginClients_.size(), 0);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, Initialize_Success) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        // 测试正常初始化, not enabled
        Status result = pluginMgr.Initialize(notEnablePlugins_).Get();
        EXPECT_FALSE(result.IsError());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, Initialize_FailWithNonCriticalPlugin) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        // 测试非关键插件初始化失败, type not support
        Status result = pluginMgr.Initialize(normalPlugins_).Get();
        EXPECT_FALSE(result.IsError()); // 非关键插件失败不应影响整体
    }

    TEST_F(VirtualEnvPluginMgrActorTest, Initialize_FailWithCriticalPlugin) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        // 测试关键插件初始化失败
        Status result = pluginMgr.Initialize(criticalPlugins_).Get();
        EXPECT_TRUE(result.IsError());
        EXPECT_EQ(result.GetMessage(), "[plugin type is not supported]");
    }

    TEST_F(VirtualEnvPluginMgrActorTest, Initialize_InitRemoteClient) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        Status result = pluginMgr.Initialize(initSuccessPlugins_).Get();
        EXPECT_FALSE(result.IsError());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, TestGetPluginID) {
        auto request = std::make_shared<messages::DeployInstanceRequest>();
        request->mutable_createoptions()->insert({VIRTUALENV_KIND, "venv"});
        request->set_traceid("trace1");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_traceid(request->traceid());
        pluginRequest->set_method(METHOD_PREPARE);
        pluginRequest->mutable_payload()->PackFrom(*request);

        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        EXPECT_EQ(pluginMgr.GetPluginID(pluginRequest).Get(), "venvPlugin");
        request->mutable_createoptions()->operator[](VIRTUALENV_KIND) = "not_exist";
        pluginRequest->mutable_payload()->PackFrom(*request);

        EXPECT_EQ(pluginMgr.GetPluginID(pluginRequest).Get(), "");
    }

    TEST_F(VirtualEnvPluginMgrActorTest, TestIsHealthy) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        pluginMgr.healthyCheckResults_["healthy"] = true;
        pluginMgr.healthyCheckResults_["unHealthy"] = false;

        EXPECT_FALSE(pluginMgr.IsHealthy("pluginNotExist"));
        EXPECT_TRUE(pluginMgr.IsHealthy("healthy"));
        EXPECT_FALSE(pluginMgr.IsHealthy("unHealthy"));
    }


    TEST_F(VirtualEnvPluginMgrActorTest, RecoverCache_InputInvalid) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        EXPECT_FALSE(pluginMgr.RecoverCache("invalid").Get());

        EXPECT_EQ(pluginMgr.envReferInfos_.size(), 0);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, RecoverCache_RuntimeInfoEmpty) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        messages::RegisterRuntimeManagerRequest req;
        std::string message;

        EXPECT_TRUE(req.SerializeToString(&message));
        EXPECT_TRUE(pluginMgr.RecoverCache(message).Get());

        EXPECT_EQ(pluginMgr.envReferInfos_.size(), 0);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, RecoverCache_AllRuntimeInfoSuccess) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        messages::RegisterRuntimeManagerRequest req;

        messages::RuntimeInstanceInfo runtimeInstanceInfo;
        runtimeInstanceInfo.set_instanceid("TEST_INSTANCE_ID");
        runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_NAME, "env1");
        runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_KIND, "venv");


        messages::RuntimeInstanceInfo runtimeInstanceInfo2;
        runtimeInstanceInfo2.set_instanceid("TEST_INSTANCE_ID2");
        runtimeInstanceInfo2.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_NAME, "env2");
        runtimeInstanceInfo2.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_KIND, "venv");


        req.mutable_runtimeinstanceinfos()->insert({"runtimeID1", runtimeInstanceInfo});
        req.mutable_runtimeinstanceinfos()->insert({"runtimeID2", runtimeInstanceInfo2});

        std::string message;

        EXPECT_TRUE(req.SerializeToString(&message));
        EXPECT_TRUE(pluginMgr.RecoverCache(message).Get());
        EXPECT_EQ(pluginMgr.envReferInfos_.size(), 2);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, RecoverCache_PartialRuntimeInfoSuccess) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        messages::RegisterRuntimeManagerRequest req;

        messages::RuntimeInstanceInfo runtimeInstanceInfo;
        runtimeInstanceInfo.set_instanceid("TEST_INSTANCE_ID");
        runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_NAME, "env1");
        runtimeInstanceInfo.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_KIND, "venv");

        messages::RuntimeInstanceInfo runtimeInstanceInfo2;
        runtimeInstanceInfo2.set_instanceid("TEST_INSTANCE_ID2");
        // 少了 virtual env kind
        runtimeInstanceInfo2.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_NAME, "env2");

        messages::RuntimeInstanceInfo runtimeInstanceInfo3;
        runtimeInstanceInfo3.set_instanceid("TEST_INSTANCE_ID3");
        // 少了 virtual env name
        runtimeInstanceInfo3.mutable_deploymentconfig()->mutable_deployoptions()->emplace(
            VIRTUALENV_KIND, "venv");


        req.mutable_runtimeinstanceinfos()->insert({"runtimeID1", runtimeInstanceInfo});
        req.mutable_runtimeinstanceinfos()->insert({"runtimeID2", runtimeInstanceInfo2});
        req.mutable_runtimeinstanceinfos()->insert({"runtimeID3", runtimeInstanceInfo3});

        std::string message;

        EXPECT_TRUE(req.SerializeToString(&message));
        EXPECT_TRUE(pluginMgr.RecoverCache(message).Get());
        
        EXPECT_EQ(pluginMgr.envReferInfos_.size(), 1);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_VirtualEnvManager_EnvRef_Success) {
        // ------ TEST Increase -----
        auto request = std::make_shared<messages::DeployInstanceRequest>();
        request->mutable_createoptions()->insert({VIRTUALENV_KIND, "venv"});
        request->mutable_createoptions()->insert({VIRTUALENV_NAME, "testVenv"});
        request->set_traceid("trace1");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_traceid(request->traceid());
        pluginRequest->set_method(METHOD_INCREASE_REF);
        pluginRequest->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});
        pluginRequest->mutable_payload()->PackFrom(*request);

        std::shared_ptr<agent_plugin::PluginResponse> response;
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");
        Status status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest, response).Get();
        EXPECT_TRUE(status.IsOk()); {
            EXPECT_EQ(pluginMgr.envReferInfos_.size(), 1);
            EnvInfo envInfo = pluginMgr.envReferInfos_.at("testVenv_venv");
            EXPECT_EQ(envInfo.envName, "testVenv");
            EXPECT_EQ(envInfo.envType, "venv");
            EXPECT_EQ(envInfo.pluginID, "venvPlugin");
            EXPECT_EQ(envInfo.runtimeIds.size(), 1);
        }

        // another runtimeID use the same virtual env, runtimeIds add runtimeID2
        pluginRequest->mutable_metadata()->operator[](CONSTANT_RUNTIME_ID) = "runtimeID2";
        status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest, response).Get();
        EXPECT_TRUE(status.IsOk()); {
            EXPECT_EQ(pluginMgr.envReferInfos_.size(), 1);
            EnvInfo envInfo = pluginMgr.envReferInfos_.at("testVenv_venv");
            EXPECT_EQ(envInfo.runtimeIds.size(), 2);
        }

        // another runtimeID use different virtual env, envReferInfos_ add runtimeID3
        pluginRequest->mutable_metadata()->operator[](CONSTANT_RUNTIME_ID) = "runtimeID3";
        request->mutable_createoptions()->operator[](VIRTUALENV_NAME) = "testVenv2";

        pluginRequest->mutable_payload()->PackFrom(*request);
        status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest, response).Get();
        EXPECT_TRUE(status.IsOk()); {
            EXPECT_EQ(pluginMgr.envReferInfos_.size(), 2);
            EnvInfo envInfo = pluginMgr.envReferInfos_.at("testVenv2_venv");
            EXPECT_EQ(envInfo.envName, "testVenv2");
            EXPECT_EQ(envInfo.runtimeIds.size(), 1);
        }

        // ------ TEST Decrease -----
        auto pluginRequestDecrease = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequestDecrease->set_traceid("traceID");
        pluginRequestDecrease->set_method(METHOD_DECREASE_REF);
        pluginRequestDecrease->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});

        status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequestDecrease, response).Get();
        EXPECT_TRUE(status.IsOk()); {
            EXPECT_EQ(pluginMgr.envReferInfos_.size(), 2);
            EnvInfo envInfo = pluginMgr.envReferInfos_.at("testVenv_venv");
            EXPECT_EQ(envInfo.envName, "testVenv");
            EXPECT_EQ(envInfo.runtimeIds.size(), 1);
        }

        pluginRequestDecrease->mutable_metadata()->operator[](CONSTANT_RUNTIME_ID) = "runtimeID2";
        status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequestDecrease, response).Get();
        EXPECT_TRUE(status.IsOk()); {
            EXPECT_EQ(pluginMgr.envReferInfos_.size(), 2);
            EnvInfo envInfo = pluginMgr.envReferInfos_.at("testVenv_venv");
            EXPECT_EQ(envInfo.envName, "testVenv");
            // runtimeIds不存在了，但是envReferInfos_里还有EnvInfo对象，定时任务回收
            EXPECT_EQ(envInfo.runtimeIds.size(), 0);
        }

        pluginRequestDecrease->mutable_metadata()->operator[](CONSTANT_RUNTIME_ID) = "runtimeID3";
        status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequestDecrease, response).Get();
        EXPECT_TRUE(status.IsOk()); {
            EXPECT_EQ(pluginMgr.envReferInfos_.size(), 2);
            EnvInfo envInfo = pluginMgr.envReferInfos_.at("testVenv2_venv");
            EXPECT_EQ(envInfo.envName, "testVenv2");
            EXPECT_EQ(envInfo.runtimeIds.size(), 0);
        }

        // ------ TEST Not support -----
        auto pluginRequestNotSupport = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequestNotSupport->set_traceid("traceID");
        pluginRequestNotSupport->set_method("METHOD_NOT_SUPPORT");
        status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequestNotSupport, response).Get();
        EXPECT_FALSE(status.IsOk());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_SpecificPlugin_MethodNotFound) {
        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_method("Method_Not_Found");
        pluginRequest->mutable_metadata()->insert({ CONSTANT_RUNTIME_ID, "runtimeID" });

        std::shared_ptr<agent_plugin::PluginResponse> response;
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        Status status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest, response).Get();
        EXPECT_FALSE(status.IsOk());
        EXPECT_EQ(status.GetMessage(), "[method (Method_Not_Found) is not supported.]");
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_VirtualEnvManager_EnvRef_Failed) {
        // not DeployInstanceRequest, invalid
        auto request = std::make_shared<messages::DeployInstanceResponse>();
        request->set_instanceid("instance_id");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_method(METHOD_INCREASE_REF);
        pluginRequest->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});
        pluginRequest->mutable_payload()->PackFrom(*request);

        std::shared_ptr<agent_plugin::PluginResponse> response;
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        Status status = pluginMgr.CallPlugin(CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest, response).Get();
        EXPECT_FALSE(status.IsOk());
        EXPECT_EQ(status.GetMessage(), "[failed to unpack PluginRequest to deployInstanceRequest]");
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_SpecificPlugin_MethodNotSupport) {
        auto request = std::make_shared<messages::DeployInstanceRequest>();
        request->set_instanceid("instance_id");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_method("METHOD_NOT_SUPPORT");
        pluginRequest->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});
        pluginRequest->mutable_payload()->PackFrom(*request);

        std::shared_ptr<agent_plugin::PluginResponse> response;
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");
        pluginMgr.healthyCheckResults_["pluginID"] = true;

        Status status = pluginMgr.CallPlugin("pluginID", pluginRequest, response).Get();
        EXPECT_FALSE(status.IsOk());
        EXPECT_EQ(status.GetMessage(), "[method (METHOD_NOT_SUPPORT) is not supported.]");
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_SpecificPlugin_ClientNotFound) {
        auto request = std::make_shared<messages::DeployInstanceRequest>();
        request->set_instanceid("instance_id");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_method(METHOD_PREPARE);
        pluginRequest->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});
        pluginRequest->mutable_payload()->PackFrom(*request);

        std::shared_ptr<agent_plugin::PluginResponse> response;
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");
        pluginMgr.healthyCheckResults_["pluginID"] = true;

        Status status = pluginMgr.CallPlugin("pluginID", pluginRequest, response).Get();
        EXPECT_EQ(status.StatusCode(), StatusCode::POINTER_IS_NULL);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_SpecificPlugin_RequestInValid) {
        // not DeployInstanceRequest, invalid
        auto request = std::make_shared<messages::DeployInstanceResponse>();
        request->set_instanceid("instance_id");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_method(METHOD_PREPARE);
        pluginRequest->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});
        pluginRequest->mutable_payload()->PackFrom(*request);

        std::shared_ptr<agent_plugin::PluginResponse> response;
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");
        pluginMgr.pluginClients_["pluginID"] = std::make_shared<MockPluginClient>();
        pluginMgr.healthyCheckResults_["pluginID"] = true;

        Status status = pluginMgr.CallPlugin("pluginID", pluginRequest, response).Get();
        EXPECT_FALSE(status.IsOk());
        EXPECT_EQ(status.GetMessage(), "[failed to unpack PluginRequest to deployInstanceRequest]");
    }

    TEST_F(VirtualEnvPluginMgrActorTest, CallPlugin_SpecificPlugin_Success) {
        auto request = std::make_shared<messages::DeployInstanceRequest>();
        request->set_instanceid("instance_id");

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_method(METHOD_PREPARE);
        pluginRequest->mutable_metadata()->insert({CONSTANT_RUNTIME_ID, "runtimeID"});
        pluginRequest->mutable_payload()->PackFrom(*request);

        MockPluginClient mockPluginClient;
        EXPECT_CALL(mockPluginClient, Call(testing::_, testing::_)).WillOnce(testing::Return(Status::OK()));

        auto pluginClient = std::shared_ptr<MockPluginClient>(
            &mockPluginClient,
            [](auto) {
            }
        );

        auto pluginMgr = std::make_shared<VirtualEnvPluginMgrActor>("test_actor");
        litebus::Spawn(pluginMgr);
        pluginMgr->pluginClients_["pluginID"] = std::move(pluginClient);
        pluginMgr->healthyCheckResults_["pluginID"] = true;

        std::shared_ptr<agent_plugin::PluginResponse> response;
        Status status = pluginMgr->CallPlugin("pluginID", pluginRequest, response).Get();
        EXPECT_TRUE(status.IsOk());

        litebus::Terminate(pluginMgr->GetAID());
        litebus::Await(pluginMgr->GetAID());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, DoClearVirtualEnv_GetEnvNameNeed2ClearEmtpy) {
        MockPluginClient mockPluginClient;
        EXPECT_CALL(mockPluginClient, Call(testing::_, testing::_)).Times(0);

        auto pluginClient = std::shared_ptr<MockPluginClient>(
            &mockPluginClient,
            [](auto) {
            }
        );

        auto pluginMgr = std::make_shared<VirtualEnvPluginMgrActor>("test_actor");
        litebus::Spawn(pluginMgr);
        pluginMgr->pluginClients_["pluginID"] = std::move(pluginClient);
        pluginMgr->virtualEnvIdleTime_ = 10000;

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();

        // envIdleExceedLimit == false
        pluginMgr->envReferInfos_.emplace("envName_envType", EnvInfo{{}, "pluginID", "envName", "envType"});
        // envIdle == false
        pluginMgr->envReferInfos_.emplace("envName2_envType", EnvInfo{
                                             {"runtimeID"}, "pluginID", "envName2", "envType",
                                             std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                                         });
        EXPECT_EQ(pluginMgr->envReferInfos_.size(), 2);
        pluginMgr->DoClearVirtualEnv();
        EXPECT_EQ(pluginMgr->envReferInfos_.size(), 2);
        litebus::Terminate(pluginMgr->GetAID());
        litebus::Await(pluginMgr->GetAID());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, DoClearVirtualEnv_CallScucess) {
        MockPluginClient mockPluginClient;
        EXPECT_CALL(mockPluginClient, Call(testing::_, testing::_)).WillOnce(testing::Return(Status::OK()));

        auto pluginClient = std::shared_ptr<MockPluginClient>(
            &mockPluginClient,
            [](auto) {
            }
        );

        auto pluginMgr = std::make_shared<VirtualEnvPluginMgrActor>("test_actor");
        litebus::Spawn(pluginMgr);
        pluginMgr->virtualEnvIdleTime_ = 0;
        pluginMgr->healthyCheckResults_["pluginID"] = true;
        pluginMgr->pluginClients_["pluginID"] = std::move(pluginClient);

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();

        pluginMgr->envReferInfos_.emplace("envName_envType", EnvInfo{
                                             {}, "pluginID", "envName", "envType",
                                             std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                                         });
        EXPECT_EQ(pluginMgr->envReferInfos_.size(), 1);
        pluginMgr->DoClearVirtualEnv();
        litebus::Terminate(pluginMgr->GetAID());
        litebus::Await(pluginMgr->GetAID());
        EXPECT_EQ(pluginMgr->envReferInfos_.size(), 0);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, DoClearVirtualEnv_CallError) {
        MockPluginClient mockPluginClient;
        EXPECT_CALL(mockPluginClient, Call(testing::_, testing::_)).WillOnce(
            testing::Return(Status(StatusCode::FAILED)));

        auto pluginClient = std::shared_ptr<MockPluginClient>(
            &mockPluginClient,
            [](auto) {
            }
        );

        auto pluginMgr = std::make_shared<VirtualEnvPluginMgrActor>("test_actor");
        litebus::Spawn(pluginMgr);
        pluginMgr->virtualEnvIdleTime_ = 0;
        pluginMgr->healthyCheckResults_["pluginID"] = true;
        pluginMgr->pluginClients_["pluginID"] = std::move(pluginClient);

        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();

        pluginMgr->envReferInfos_.emplace("envName_envType", EnvInfo{
                                             {}, "pluginID", "envName", "envType",
                                             std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                                         });
        EXPECT_EQ(pluginMgr->envReferInfos_.size(), 1);
        pluginMgr->DoClearVirtualEnv();
        EXPECT_EQ(pluginMgr->envReferInfos_.size(), 1);

        litebus::Terminate(pluginMgr->GetAID());
        litebus::Await(pluginMgr->GetAID());
    }

    TEST_F(VirtualEnvPluginMgrActorTest, HealthCheck_NormalFlow) {
        agent_plugin::PluginResponse response;
        response.set_code(0); // 成功状态码

        MockPluginClient mockPluginClient;
        EXPECT_CALL(mockPluginClient, Call(testing::_, testing::_))
                .WillRepeatedly(testing::DoAll(testing::SetArgReferee<1>(response), testing::Return(Status::OK())));

        auto pluginClient = std::shared_ptr<MockPluginClient>(
            &mockPluginClient,
            [](auto) {
            }
        );

        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");
        pluginMgr.pluginClients_["pluginID"] = std::move(pluginClient);
        pluginMgr.healthCheckInterval_ = 1000;

        PluginInfo pluginInfo;
        pluginInfo.id = "pluginID";
        pluginMgr.pluginConfigs_[pluginInfo.id] = std::move(pluginInfo);

        // 未初始化，healthyResult=false
        EXPECT_FALSE(pluginMgr.healthyCheckResults_["pluginID"]);

        pluginMgr.HealthCheck();
        // 等待一段时间让健康检查执行
        std::this_thread::sleep_for(std::chrono::seconds(2));


        EXPECT_TRUE(pluginMgr.healthyCheckResults_["pluginID"]);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, HealthCheck_NullPluginClient) {
        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");

        pluginMgr.healthCheckInterval_ = 1000;

        PluginInfo pluginInfo;
        pluginInfo.id = "pluginID";
        pluginMgr.pluginConfigs_[pluginInfo.id] = std::move(pluginInfo);

        // 未初始化，healthyResult=false
        EXPECT_FALSE(pluginMgr.healthyCheckResults_["pluginID_not_exist"]);

        pluginMgr.HealthCheck();
        // 等待一段时间让健康检查执行
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EXPECT_FALSE(pluginMgr.healthyCheckResults_["pluginID_not_exist"]);
    }

    TEST_F(VirtualEnvPluginMgrActorTest, HealthCheck_HealthCheckFailed) {
        agent_plugin::PluginResponse response;
        response.set_code(1); // 异常状态码

        MockPluginClient mockPluginClient;
        EXPECT_CALL(mockPluginClient, Call(testing::_, testing::_))
                .WillRepeatedly(testing::DoAll(testing::SetArgReferee<1>(response), testing::Return(Status(StatusCode::FAILED))));

        auto pluginClient = std::shared_ptr<MockPluginClient>(
            &mockPluginClient,
            [](auto) {
            }
        );

        VirtualEnvPluginMgrActor pluginMgr("virtual_mgr_actor");
        pluginMgr.pluginClients_["pluginID"] = std::move(pluginClient);
        pluginMgr.healthCheckInterval_ = 1000;

        PluginInfo pluginInfo;
        pluginInfo.id = "pluginID";
        pluginMgr.pluginConfigs_[pluginInfo.id] = std::move(pluginInfo);

        // 未初始化，healthyResult=false
        EXPECT_FALSE(pluginMgr.healthyCheckResults_["pluginID"]);

        pluginMgr.HealthCheck();
        // 等待一段时间让健康检查执行
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EXPECT_FALSE(pluginMgr.healthyCheckResults_["pluginID"]);
    }
}
