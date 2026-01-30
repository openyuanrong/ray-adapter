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

#include "multi_plugin_client.h"

#include <nlohmann/json.hpp>

#include "common/constants/actor_name.h"
#include "common/constants/constants.h"
#include "common/logs/logging.h"
#include "common/status/status.h"
#include "function_agent/plugin/config_parse_util.h"
#include "function_agent/plugin/plugin_config.h"
#include "function_agent/plugin/virtualenv_plugin_mgr_actor.h"

namespace functionsystem::function_agent {
MultiPluginClient::MultiPluginClient()
{
    virtualEnvMgrActor_ = std::make_shared<VirtualEnvPluginMgrActor>(FUNCTION_AGENT_VIRTUAL_ENV_MGR_ACTOR_NAME);
    litebus::Spawn(virtualEnvMgrActor_);
}
MultiPluginClient::~MultiPluginClient()
{
    litebus::Terminate(virtualEnvMgrActor_->GetAID());
    litebus::Await(virtualEnvMgrActor_->GetAID());
}

litebus::Future<Status> MultiPluginClient::RegisterPlugin(const std::string &config)
{
    const auto parsedConfig = ParsePluginsConfig(config);
    pluginGroupMgrs_.insert({ GROUP_NAME_VIRTUAL_ENV_PLUGINS, virtualEnvMgrActor_ });
    auto virtualEnvGroupConfigs = GetPluginConfigOfGroup(parsedConfig, GROUP_NAME_VIRTUAL_ENV_PLUGINS);
    return litebus::Async(virtualEnvMgrActor_->GetAID(), &VirtualEnvPluginMgrActor::LoadPlugin, virtualEnvGroupConfigs)
        .OnComplete([this, parsedConfig](const litebus::Future<Status> statusFut) -> litebus::Future<Status> {
            if (statusFut.IsError()) {
                return statusFut;
            }
            auto status = statusFut.Get();
            if (status.IsError()) {
                return status;
            }
            for (const auto &[groupName, pluginConfigs] : parsedConfig.pluginGroups) {
                if (auto it = pluginGroupMgrs_.find(groupName); it != pluginGroupMgrs_.end()) {
                    for (const auto &pluginCfg : pluginConfigs) {
                        if (pluginCfg.enabled) {
                            pluginMgrs_[pluginCfg.id] = it->second;
                        }
                    }
                } else {
                    // manager沒有注冊，无需关注插件状态
                    YRLOG_INFO("No manager registered for plugin group {}", groupName);
                }
            }
            return Status::OK();
        });
}

litebus::Future<bool> MultiPluginClient::RecoverPluginCache(const std::string &message)
{
    if (pluginGroupMgrs_.empty()) {
        return true;
    }
    return litebus::Async(virtualEnvMgrActor_->GetAID(), &VirtualEnvPluginMgrActor::RecoverCache, message);
}

namespace {
void CreateVirtualEnvNameIfNeeded(
    const std::shared_ptr<messages::DeployInstanceRequest> &request,
    std::unordered_map<std::string, std::string> &requestID2VenvName)
{
    if (auto it = request->createoptions().find(VIRTUALENV_NAME); it == request->createoptions().end()) {
        const std::string envName = litebus::uuid_generator::UUID::GetRandomUUID().ToString();
        YRLOG_DEBUG("{}|{}|virtual env name is empty, create new one: {}", request->traceid(), request->requestid(),
                    envName);
        request->mutable_createoptions()->insert({VIRTUALENV_NAME, envName});
        requestID2VenvName.insert({request->requestid(), envName});
    }
}

std::shared_ptr<agent_plugin::PluginRequest> BuildPluginRequestForPrepare(
    const std::shared_ptr<messages::DeployInstanceRequest> &request)
{
    auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
    pluginRequest->set_traceid(request->traceid());
    pluginRequest->set_requestid(request->requestid());
    pluginRequest->set_method(METHOD_PREPARE);
    pluginRequest->mutable_payload()->PackFrom(*request);
    return pluginRequest;
}

Status ProcessPrepareResponse(
    const std::shared_ptr<agent_plugin::PluginResponse> &pluginResponse,
    const std::shared_ptr<messages::DeployInstanceRequest> &request)
{
    agent_plugin::PrepareResponse prepareResponse;
    if (!pluginResponse->payload().UnpackTo(&prepareResponse)) {
        YRLOG_WARN("{}|{}|failed to unpack PluginResponse to prepareResponse", request->traceid(),
                   request->requestid());
        return Status(StatusCode::FAILED, "invalid response");
    }
    YRLOG_DEBUG("{}|{}|message {}", request->traceid(), request->requestid(), prepareResponse.message());
    for (const auto &[fst, snd] : prepareResponse.cmds()) {
        request->mutable_createoptions()->insert({fst, snd});
        YRLOG_DEBUG("{}|{}|cmds key {}, value {}", request->traceid(), request->requestid(), fst, snd);
    }
    return Status::OK();
}
}  // namespace

litebus::Future<Status> MultiPluginClient::PrepareVirtualEnv(
    const std::shared_ptr<messages::DeployInstanceRequest> &request)
{
    YRLOG_DEBUG("{}|{}|start to prepare virtual env", request->traceid(), request->requestid());
    if (auto it = request->createoptions().find(VIRTUALENV_KIND); it == request->createoptions().end()) {
        return Status::OK();
    }
    const auto virtualEnvPluginMgrIter = pluginGroupMgrs_.find(GROUP_NAME_VIRTUAL_ENV_PLUGINS);
    if (virtualEnvPluginMgrIter == pluginGroupMgrs_.end()) {
        YRLOG_ERROR("{}|{}|virtual env plugin manager not found, can not prepare env", request->traceid(),
                    request->requestid());
        return Status(StatusCode::FAILED, "virtual env plugin manager not found");
    }
    CreateVirtualEnvNameIfNeeded(request, requestID2VenvName_);
    auto pluginRequest = BuildPluginRequestForPrepare(request);
    auto pluginResponse = std::make_shared<agent_plugin::PluginResponse>();
    const auto manager = virtualEnvPluginMgrIter->second;
    auto actorPtr = std::dynamic_pointer_cast<litebus::ActorBase>(manager);
    if (!actorPtr) {
        YRLOG_INFO("{}|{}|not found virtual env manager for {}", request->traceid(), request->requestid());
        return Status(StatusCode::FAILED, "virtual env plugin manager not found");
    }
    return litebus::Async(actorPtr->GetAID(), &PluginMgr::GetPluginID, pluginRequest)
        .Then([=](const std::string &pluginID) -> litebus::Future<Status> {
            YRLOG_INFO("{}|get plugin ID {} for request {}", request->traceid(), pluginID, request->requestid());
            if (pluginID.empty()) {
                return Status(StatusCode::FAILED, "get plugin ID failed");
            }
            return Call(pluginID, pluginRequest, pluginResponse);
        })
        .OnComplete([=](const litebus::Future<Status> &callResultFuture) -> litebus::Future<Status> {
            if (callResultFuture.IsError()) {
                return Status(StatusCode::FAILED, "callResultFuture Error");
            }
            auto callResult = callResultFuture.Get();
            if (callResult.IsError()) {
                YRLOG_ERROR("{}|{}|call plugin failed, err {}", request->traceid(), request->requestid(),
                            callResult.GetMessage());
                return callResult;
            }
            return ProcessPrepareResponse(pluginResponse, request);
        });
}

void MultiPluginClient::IncreaseVirtualEnvRef(const std::shared_ptr<messages::DeployInstanceRequest> &request,
                                              const std::string &runtimeID)
{
    if (auto it = request->createoptions().find(VIRTUALENV_KIND); it != request->createoptions().end()) {
        YRLOG_INFO("{}|{}|start add env ref with runtime {}", request->traceid(), request->requestid(), runtimeID);
        const auto virtualEnvPluginMgrIter = pluginGroupMgrs_.find(GROUP_NAME_VIRTUAL_ENV_PLUGINS);
        if (virtualEnvPluginMgrIter == pluginGroupMgrs_.end()) {
            YRLOG_WARN(
                "{}|{}|virtual env plugin manager not found, can not increase virtual env reference with runtime {}",
                request->traceid(), request->requestid(), runtimeID);
            return;
        }
        if (auto iter = requestID2VenvName_.find(request->requestid()); iter != requestID2VenvName_.end()) {
            YRLOG_DEBUG("{}|{}|use recorded envName {} of runtime {}", request->traceid(), request->requestid(),
                        iter->second, runtimeID);
            // if envName exist, will not be replaced
            request->mutable_createoptions()->insert({ VIRTUALENV_NAME, iter->second });
        }
        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_traceid(request->traceid());
        pluginRequest->set_requestid(request->requestid());
        pluginRequest->set_method(METHOD_INCREASE_REF);
        pluginRequest->mutable_payload()->PackFrom(*request);
        pluginRequest->mutable_metadata()->insert({ CONSTANT_RUNTIME_ID, runtimeID });

        auto pluginResponse = std::make_shared<agent_plugin::PluginResponse>();
        requestID2VenvName_.erase(request->requestid());
        const auto manager = virtualEnvPluginMgrIter->second;
        if (auto actorPtr = std::dynamic_pointer_cast<litebus::ActorBase>(manager)) {
            litebus::Async(actorPtr->GetAID(), &PluginMgr::CallPlugin, CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest,
                           pluginResponse);
            return;
        }
        YRLOG_INFO("{}|{}|not found virtual env manager for {}", request->traceid(), request->requestid(), runtimeID);
    }
}

void MultiPluginClient::DecreaseVirtualEnvRef(const std::string &runtimeID, const messages::RuntimeInstanceInfo &info)
{
    if (auto it = info.deploymentconfig().deployoptions().find(VIRTUALENV_KIND);
        it != info.deploymentconfig().deployoptions().end()) {
        YRLOG_INFO("{}|{}|start remove env ref with runtime {}", info.traceid(), info.requestid(), runtimeID);

        const auto virtualEnvPluginMgrIter = pluginGroupMgrs_.find(GROUP_NAME_VIRTUAL_ENV_PLUGINS);
        if (virtualEnvPluginMgrIter == pluginGroupMgrs_.end()) {
            YRLOG_WARN(
                "{}|{}|virtual env plugin manager not found, can not increase virtual env reference with runtime {}",
                info.traceid(), info.requestid(), runtimeID);
            return;
        }

        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_traceid(info.traceid());
        pluginRequest->set_requestid(info.requestid());
        pluginRequest->set_method(METHOD_DECREASE_REF);
        pluginRequest->mutable_metadata()->insert({ CONSTANT_RUNTIME_ID, runtimeID });

        auto pluginResponse = std::make_shared<agent_plugin::PluginResponse>();
        const auto manager = virtualEnvPluginMgrIter->second;
        if (auto actorPtr = std::dynamic_pointer_cast<litebus::ActorBase>(manager)) {
            litebus::Async(actorPtr->GetAID(), &PluginMgr::CallPlugin, CONSTANT_VIRTUALENV_PLUGIN_MGR, pluginRequest,
                           pluginResponse);
            return;
        }
        YRLOG_INFO("{}|{}|not found virtual env manager for {}", info.traceid(), info.requestid(), runtimeID);
    }
}

litebus::Future<Status> MultiPluginClient::PrepareEnv(const std::shared_ptr<messages::DeployInstanceRequest> &request)
{
    // we can use PrepareVirtualEnv().Then(PrepareXXXEnv).Thenxxxxx
    return PrepareVirtualEnv(request);
}

void MultiPluginClient::IncreaseEnvRef(const std::shared_ptr<messages::DeployInstanceRequest> &request,
                                       const std::string &runtimeID)
{
    IncreaseVirtualEnvRef(request, runtimeID);
}

void MultiPluginClient::DecreaseEnvRef(const std::string &runtimeID, const messages::RuntimeInstanceInfo &info)
{
    DecreaseVirtualEnvRef(runtimeID, info);
}

litebus::Future<Status> MultiPluginClient::Call(const std::string &pluginID,
                                                std::shared_ptr<agent_plugin::PluginRequest> request,
                                                std::shared_ptr<agent_plugin::PluginResponse> response)
{
    const auto it = pluginMgrs_.find(pluginID);
    if (it == pluginMgrs_.end()) {
        YRLOG_ERROR("{}|{}|No manager registered for plugin {}", request->traceid(), request->requestid(), pluginID);
        return Status(StatusCode::FAILED, "No manager registered for plugin");
    }
    const auto manager = it->second;
    if (auto actorPtr = std::dynamic_pointer_cast<litebus::ActorBase>(manager)) {
        return litebus::Async(actorPtr->GetAID(), &PluginMgr::CallPlugin, pluginID, request, response);
    }
    YRLOG_ERROR("{}|{}|internal system error when call plugin {}", request->traceid(), request->requestid(), pluginID);
    return Status(StatusCode::FAILED, "internal error");
}
}  // namespace functionsystem::function_agent
