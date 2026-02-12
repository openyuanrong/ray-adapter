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

#include "virtualenv_plugin_mgr_actor.h"

#include "async/async.hpp"
#include "async/asyncafter.hpp"
#include "common/constants/actor_name.h"
#include "common/constants/constants.h"
#include "common/logs/logging.h"
#include "common/proto/pb/message_pb.h"
#include "common/utils/time_utils.h"
#include "function_agent/plugin/process_util.h"
#include "function_agent/plugin/remote_plugin_client.h"

namespace functionsystem::function_agent {
std::unordered_map<std::string, std::string> KEY_WORDS_TO_PLUGIN_ID_MAP = { { "venv", "venvPlugin" } };

inline std::string GetValueOfDeployOptionsMap(const google::protobuf::Map<std::string, std::string> &map,
                                              const std::string &key)
{
    if (auto it = map.find(key); it != map.end()) {
        return it->second;
    }
    return "";
}

inline EnvInfo ConstructEnvInfoFromRequestMap(const google::protobuf::Map<std::string, std::string> &map)
{
    auto envName = GetValueOfDeployOptionsMap(map, VIRTUALENV_NAME);
    auto envType = GetValueOfDeployOptionsMap(map, VIRTUALENV_KIND);
    std::string pluginID = "";

    if (auto pluginIDIter = KEY_WORDS_TO_PLUGIN_ID_MAP.find(envType);
        pluginIDIter != KEY_WORDS_TO_PLUGIN_ID_MAP.end()) {
        pluginID = pluginIDIter->second;
    }
    return EnvInfo{ {}, pluginID, envName, envType };
}

void VirtualEnvPluginMgrActor::Init()
{
    ActorBase::Init();
}

VirtualEnvPluginMgrActor::VirtualEnvPluginMgrActor(const std::string &name) : ActorBase(name)
{
}
VirtualEnvPluginMgrActor::~VirtualEnvPluginMgrActor()
{
}

litebus::Future<Status> VirtualEnvPluginMgrActor::LoadPlugin(const PluginGroup &pluginGroup)
{
    if (pluginGroup.empty()) {
        return Status::OK();
    }
    for (const auto &pluginInfo : pluginGroup) {
        // 加载失败&&关键插件，提前返回
        auto result = LoadPlugin(pluginInfo);
        if (result.IsError() && pluginInfo.critical) {
            return result;
        }
    }
    return Initialize(pluginGroup);
}

litebus::Future<Status> VirtualEnvPluginMgrActor::Initialize(const PluginGroup &pluginGroup)
{
    for (const auto &pluginInfo : pluginGroup) {
        auto result = InitPlugin(pluginInfo);
        if (result.IsError() && pluginInfo.critical) {
            return result;
        }
    }
    litebus::AsyncAfter(envRefRecycleInterval_, GetAID(), &VirtualEnvPluginMgrActor::RecycleUnusedEnvs);
    litebus::AsyncAfter(healthCheckInterval_, GetAID(), &VirtualEnvPluginMgrActor::HealthCheck);
    return Status::OK();
}

void VirtualEnvPluginMgrActor::Finalize()
{
    (void)litebus::TimerTools::Cancel(recycleUnusedEnvsTimer_);
    (void)litebus::TimerTools::Cancel(healthCheckTimer_);
    for (const auto &pluginConfigPair : pluginConfigs_) {
        auto pluginID = pluginConfigPair.first;
        UnloadPlugin(pluginID);
    }
    pluginConfigs_.clear();
    envReferInfos_.clear();
    ActorBase::Finalize();
}

litebus::Future<Status> VirtualEnvPluginMgrActor::CallVirtualEnvMgrMethod(
    std::shared_ptr<agent_plugin::PluginRequest> request)
{
    auto method = request->method();
    auto runtimeID = GetValueOfDeployOptionsMap(request->metadata(), CONSTANT_RUNTIME_ID);
    if (method == METHOD_INCREASE_REF) {
        functionsystem::messages::DeployInstanceRequest deployInstanceRequest;
        if (!request->payload().UnpackTo(&deployInstanceRequest)) {
            return Status(StatusCode::FAILED, "failed to unpack PluginRequest to deployInstanceRequest");
        }
        EnvInfo envInfo = ConstructEnvInfoFromRequestMap(deployInstanceRequest.createoptions());
        AddEnvReferInfos(envInfo.envName, envInfo.envType, envInfo.pluginID, runtimeID);
        YRLOG_INFO("{}|{}|increase virtual env {}_{} ref with runtime {} succeed", request->traceid(),
                   request->requestid(), envInfo.envName, envInfo.envType, runtimeID);
    } else if (method == METHOD_DECREASE_REF) {
        RmEnvReferInfos(runtimeID);
        YRLOG_INFO("{}|{}|decrease virtual env ref with runtime {} succeed", request->traceid(), request->requestid(),
                   runtimeID);
    } else {
        return Status(StatusCode::FAILED, "method (" + method + ") is not supported.");
    }
    return Status::OK();
}

std::shared_ptr<PluginClient> VirtualEnvPluginMgrActor::GetPlugin(const std::string &pluginID)
{
    auto pluginClientIter = pluginClients_.find(pluginID);
    if (pluginClientIter == pluginClients_.end()) {
        return nullptr;
    }
    return pluginClientIter->second;
}

litebus::Future<Status> VirtualEnvPluginMgrActor::CallPluginImpl(std::shared_ptr<agent_plugin::PluginRequest> request,
                                                                 std::shared_ptr<agent_plugin::PluginResponse> response,
                                                                 std::shared_ptr<PluginClient> pluginClient)
{
    return pluginClient->Call(*request, *response);
}

litebus::Future<Status> VirtualEnvPluginMgrActor::CallSpecificPlugin(
    const std::string &pluginID, std::shared_ptr<agent_plugin::PluginRequest> request,
    std::shared_ptr<agent_plugin::PluginResponse> response)
{
    if (!IsHealthy(pluginID)) {
        YRLOG_ERROR("{}|{}|plugin {} is not healthy", request->traceid(), request->requestid(), pluginID);
        return Status(StatusCode::FAILED, "plugin is not healthy");
    }
    auto method = request->method();
    if (method == METHOD_PREPARE) {
        auto pluginClient = GetPlugin(pluginID);
        if (pluginClient == nullptr) {
            YRLOG_INFO("{}|{}|Call plugin {} failed, pluginClient is null", request->traceid(), request->requestid(),
                       pluginID);
            return Status(StatusCode::POINTER_IS_NULL);
        }
        messages::DeployInstanceRequest deployInstanceRequest;
        if (!request->payload().UnpackTo(&deployInstanceRequest)) {
            return Status(StatusCode::FAILED, "failed to unpack PluginRequest to deployInstanceRequest");
        }
        agent_plugin::PrepareRequest prepareRequest;
        prepareRequest.set_language(deployInstanceRequest.language());
        for (const std::string &str : VIRTUALENV_KEYS) {
            if (auto iter(deployInstanceRequest.createoptions().find(str));
                iter != deployInstanceRequest.createoptions().end()) {
                prepareRequest.mutable_createoptions()->insert({ str, iter->second });
            }
        }
        // delegate_download功能,暂时忽略working_dir
        auto delegateIter = deployInstanceRequest.createoptions().find(ENV_DELEGATE_DOWNLOAD);
        if (delegateIter != deployInstanceRequest.createoptions().end()) {
            prepareRequest.mutable_createoptions()->insert({ ENV_DELEGATE_DOWNLOAD, delegateIter->second });
        }

        EnvInfo envInfo = ConstructEnvInfoFromRequestMap(deployInstanceRequest.createoptions());

        const std::string envNameWithType = envInfo.envName + "_" + envInfo.envType;
        if (auto iter = envReferInfos_.find(envNameWithType); iter != envReferInfos_.end()) {
            prepareRequest.mutable_createoptions()->insert({ CONSTANT_VIRTUALENV_EXISTS, "true" });
        }

        request->mutable_payload()->PackFrom(prepareRequest);
        return litebus::Async(GetAID(), &VirtualEnvPluginMgrActor::CallPluginImpl, request, response, pluginClient);
    }
    return Status(StatusCode::FAILED, "method (" + method + ") is not supported.");
}

litebus::Future<Status> VirtualEnvPluginMgrActor::CallPlugin(const std::string &pluginID,
                                                             std::shared_ptr<agent_plugin::PluginRequest> request,
                                                             std::shared_ptr<agent_plugin::PluginResponse> response)
{
    if (pluginID == CONSTANT_VIRTUALENV_PLUGIN_MGR) {
        // manager的方法，不需要调用具体的插件
        return CallVirtualEnvMgrMethod(request);
    }
    return CallSpecificPlugin(pluginID, request, response);
}

Status VirtualEnvPluginMgrActor::LoadPlugin(const PluginInfo &pluginInfo)
{
    if (!pluginInfo.enabled) {
        YRLOG_INFO("no need to load plugin {}, plugin is enabled {}", pluginInfo.id, pluginInfo.enabled);
        return Status::OK();
    }
    // 多个插件使用统一的idleTime，只需要配置其中一个即可。配置成不一样的，按json解析顺序，最后一个生效
    SetVirtualEnvIdleTime(pluginInfo);
    const auto binaryPathIt = pluginInfo.extra_params.find(BINARY_PATH);
    if (binaryPathIt != pluginInfo.extra_params.end()) {
        auto execPtr = CreatePluginProcess(pluginInfo);
        if (execPtr == nullptr || execPtr->GetPid() == -1) {
            YRLOG_WARN("failed to create exec for plugin {}, execPtr is nullptr {}", pluginInfo.id, execPtr == nullptr);
            return Status(StatusCode::FAILED, "start plugin failed");
        }
        pluginID2ExecPtr_[pluginInfo.id] = std::move(execPtr);
    }

    pluginConfigs_[pluginInfo.id] = std::move(pluginInfo);
    YRLOG_INFO("Load plugin {} succeed", pluginInfo.id);
    return Status::OK();
}

Status VirtualEnvPluginMgrActor::InitPlugin(const PluginInfo &pluginInfo)
{
    if (!pluginInfo.enabled) {
        YRLOG_INFO("no need to init plugin {}, plugin is not enabled", pluginInfo.id);
        return Status::OK();
    }
    if (pluginInfo.type == TYPE_GRPC) {
        auto remotePluginClient = std::make_shared<RemotePluginClient>(pluginInfo.address, pluginInfo.callTimeout);
        pluginClients_[pluginInfo.id] = std::move(remotePluginClient);
    } else {
        YRLOG_ERROR("plugin type is not supported, id: {}, type: {}", pluginInfo.id, pluginInfo.type);
        return Status(StatusCode::FAILED, "plugin type is not supported");
    }

    healthyCheckResults_[pluginInfo.id] = true;
    return Status::OK();
}

void VirtualEnvPluginMgrActor::HealthCheck()
{
    for (const auto &pluginConfigPair : pluginConfigs_) {
        auto pluginID = pluginConfigPair.first;
        auto pluginClient = GetPlugin(pluginID);
        if (pluginClient == nullptr) {
            YRLOG_DEBUG("Health check of plugin {} failed, pluginClient is null", pluginID);
            healthyCheckResults_[pluginID] = false;
            continue;
        }
        const std::string traceID = litebus::uuid_generator::UUID::GetRandomUUID().ToString();
        agent_plugin::PluginRequest pluginRequest;
        pluginRequest.set_traceid(traceID);
        pluginRequest.set_method(METHOD_HEALTHCHECK);

        agent_plugin::PluginResponse pluginResponse;

        Status status = pluginClient->Call(pluginRequest, pluginResponse);
        YRLOG_DEBUG("{}|Health check of plugin {}, result is {}", traceID, pluginID, status.ToString());
        healthyCheckResults_[pluginID] = !(status.IsError());
    }
    std::set<std::string> pluginClientNeeToReload;

    for (const auto &healthyCheckResult : healthyCheckResults_) {
        if (!healthyCheckResult.second) {
            YRLOG_INFO("plugin {} is not healthy, need to reload", healthyCheckResult.first);
            pluginClientNeeToReload.insert(healthyCheckResult.first);
        }
    }

    for (const auto &pluginID : pluginClientNeeToReload) {
        UnloadPlugin(pluginID);
        const auto it = pluginConfigs_.find(pluginID);
        if (it != pluginConfigs_.end()) {
            LoadPlugin(it->second);
            InitPlugin(it->second);
        } else {
            YRLOG_ERROR("plugin {} is not in cache, we can not reload it", pluginID);
        }
    }

    healthCheckTimer_ = litebus::AsyncAfter(healthCheckInterval_, GetAID(), &VirtualEnvPluginMgrActor::HealthCheck);
}

void VirtualEnvPluginMgrActor::UnloadPlugin(const std::string &pluginID)
{
    healthyCheckResults_.erase(pluginID);
    auto pluginClient = GetPlugin(pluginID);
    if (pluginClient != nullptr) {
        pluginClient->Close();
    } else {
        YRLOG_DEBUG("maybe no need to unload plugin {}, pluginClient is null", pluginID);
    }
    auto execPtrIter = pluginID2ExecPtr_.find(pluginID);
    if (execPtrIter != pluginID2ExecPtr_.end()) {
        KillPluginProcess(execPtrIter->second->GetPid(), pluginID);
    } else {
        YRLOG_DEBUG("no need to stop plugin process, pluginID {}", pluginID);
    }
    pluginClients_.erase(pluginID);
    pluginID2ExecPtr_.erase(pluginID);
}

bool VirtualEnvPluginMgrActor::IsHealthy(const std::string &pluginID)
{
    if (pluginID == CONSTANT_VIRTUALENV_PLUGIN_MGR) {
        // manager的方法，不需要调用具体的插件
        return true;
    }
    const auto healthCheckResultsIt = healthyCheckResults_.find(pluginID);
    if (healthCheckResultsIt != healthyCheckResults_.end()) {
        return healthCheckResultsIt->second;
    }
    return false;
}

litebus::Future<bool> VirtualEnvPluginMgrActor::RecoverCache(const std::string &message)
{
    YRLOG_INFO("Start to recover cache");
    messages::RegisterRuntimeManagerRequest req;
    if (!req.ParseFromString(message)) {
        YRLOG_ERROR("failed to parse RuntimeManager register message");
        return false;
    }
    auto requestInstanceInfos = req.runtimeinstanceinfos();
    for (const auto &it : requestInstanceInfos) {
        auto runtimeID = it.first;
        auto info = it.second;
        auto envName = GetValueOfDeployOptionsMap(info.deploymentconfig().deployoptions(), VIRTUALENV_NAME);
        if (envName == "") {
            YRLOG_INFO("invalid envName, maybe runtime {} not use virtual env, envName {}", runtimeID, envName);
            continue;
        }
        EnvInfo envInfo = ConstructEnvInfoFromRequestMap(info.deploymentconfig().deployoptions());
        if (envInfo.envType == "" || envInfo.pluginID == "") {
            YRLOG_INFO("invalid env message, maybe runtime {} not use virtual env, envName {}, envType {}, pluginID {}",
                       runtimeID, envInfo.envName, envInfo.envType, envInfo.pluginID);
            continue;
        }
        YRLOG_INFO("Recover env refer info with runtimeID {}, envName {}, envType {}, pluginID {}", runtimeID,
                   envInfo.envName, envInfo.envType, envInfo.pluginID);
        AddEnvReferInfos(envInfo.envName, envInfo.envType, envInfo.pluginID, runtimeID);
    }
    YRLOG_INFO("Finish recovering cache");
    return true;
}

litebus::Future<std::string> VirtualEnvPluginMgrActor::GetPluginID(std::shared_ptr<agent_plugin::PluginRequest> request)
{
    functionsystem::messages::DeployInstanceRequest deployInstanceRequest;
    if (!request->payload().UnpackTo(&deployInstanceRequest)) {
        YRLOG_WARN("{}|{}|failed to unpack PluginRequest to deployInstanceRequest", request->traceid(),
                   request->requestid());
        return "";
    }

    // 获取关键词，通过关键词找到对应的pluginID，每种插件的关键词不一样
    if (auto it = deployInstanceRequest.createoptions().find(VIRTUALENV_KIND);
        it != deployInstanceRequest.createoptions().end()) {
        auto keyWords = it->second;
        YRLOG_DEBUG("{}|{}|GetPluginID of {}, pluginID is {}", request->traceid(), request->requestid(), it->first,
                    keyWords);
        if (auto pluginIDIter = KEY_WORDS_TO_PLUGIN_ID_MAP.find(keyWords);
            pluginIDIter != KEY_WORDS_TO_PLUGIN_ID_MAP.end()) {
            return pluginIDIter->second;
        }
    }
    return "";
}

void VirtualEnvPluginMgrActor::AddEnvReferInfos(const std::string &envName, const std::string &envType,
                                                const std::string &pluginID, const std::string &runtimeID)
{
    YRLOG_INFO("env {}_{} add one runtime {} with plugin {}", envName, envType, runtimeID, pluginID);
    const std::string envNameWithType = envName + "_" + envType;
    if (auto iter = envReferInfos_.find(envNameWithType); iter == envReferInfos_.end()) {
        (void)envReferInfos_.emplace(envNameWithType, EnvInfo{ { runtimeID }, pluginID, envName, envType });
    } else {
        (void)iter->second.runtimeIds.emplace(runtimeID);
    }
}

void VirtualEnvPluginMgrActor::RmEnvReferInfos(const std::string &runtimeId)
{
    for (auto &pair : envReferInfos_) {
        if (!pair.second.runtimeIds.count(runtimeId)) {
            continue;
        }

        pair.second.runtimeIds.erase(runtimeId);
        YRLOG_INFO("runtimeId:{} is removed on envName:{}", runtimeId, pair.first);

        if (pair.second.runtimeIds.empty()) {
            pair.second.lastRemoveTimestamp = GetCurrentTimestampMs();
        }

        break;
    }
}

void VirtualEnvPluginMgrActor::GetEnvNameNeed2Clear(std::map<std::string, EnvInfo> &pluginID2EnvInfo)
{
    for (auto envReferInfoIter = envReferInfos_.begin(); envReferInfoIter != envReferInfos_.end();) {
        auto envNameWithType = envReferInfoIter->first;
        bool envIsIdle = envReferInfoIter->second.runtimeIds.empty();
        const auto now = GetCurrentTimestampMs();
        bool envIdleExceedLimit =
            now - envReferInfoIter->second.lastRemoveTimestamp >= static_cast<uint64_t>(virtualEnvIdleTime_);
        if (!envIsIdle || !envIdleExceedLimit) {
            YRLOG_DEBUG("Env {} will not be deleted, envIsIdle {}, envIdleExceedLimit {}", envNameWithType, envIsIdle,
                        envIdleExceedLimit);
            // ref不为空，或者为空的env没超过回收时间，跳过不处理
            (void)++envReferInfoIter;
            continue;
        }
        auto pluginID = envReferInfoIter->second.pluginID;
        if (!IsHealthy(pluginID)) {
            YRLOG_WARN("plugin {} not healthy, failed to clear env {}", pluginID, envNameWithType);
            (void)++envReferInfoIter;
            continue;
        }
        pluginID2EnvInfo.insert(
            { pluginID, EnvInfo{ {}, pluginID, envReferInfoIter->second.envName, envReferInfoIter->second.envType } });
        (void)++envReferInfoIter;
    }
}

void VirtualEnvPluginMgrActor::DoClearVirtualEnv()
{
    std::map<std::string, EnvInfo> pluginID2EnvInfo;
    GetEnvNameNeed2Clear(pluginID2EnvInfo);
    for (auto pluginIDEnvInfoPair : pluginID2EnvInfo) {
        auto pluginID = pluginIDEnvInfoPair.first;
        auto envInfo = pluginIDEnvInfoPair.second;
        agent_plugin::ClearRequest clearRequest;
        clearRequest.mutable_clearoptions()->insert({ VIRTUALENV_NAME, envInfo.envName });

        const std::string traceID = litebus::uuid_generator::UUID::GetRandomUUID().ToString();
        auto pluginRequest = std::make_shared<agent_plugin::PluginRequest>();
        pluginRequest->set_traceid(traceID);
        pluginRequest->set_method(METHOD_CLEAR);
        pluginRequest->mutable_payload()->PackFrom(clearRequest);

        auto pluginResponse = std::make_shared<agent_plugin::PluginResponse>();
        auto pluginClient = GetPlugin(pluginID);
        if (pluginClient == nullptr) {
            YRLOG_WARN("Clear virtual env {}_{} with plugin {} failed, pluginClient is null", envInfo.envName,
                       envInfo.envType, pluginID);
        } else {
            litebus::Async(GetAID(), &VirtualEnvPluginMgrActor::CallPluginImpl, pluginRequest, pluginResponse,
                           pluginClient)
                .OnComplete([=](const litebus::Future<Status> &statusFut) {
                    const std::string envNameWithType = envInfo.envName + "_" + envInfo.envType;
                    if (!statusFut.IsError()) {
                        if (statusFut.Get().IsError()) {
                            YRLOG_WARN("{}|failed to recycle env {}, error message {}", traceID, envNameWithType,
                                       statusFut.Get().GetMessage());
                        } else {
                            YRLOG_INFO("{}||env {}_{} is recycled", traceID, envInfo.envName, envInfo.envType);
                            envReferInfos_.erase(envNameWithType);
                        }
                    } else {
                        YRLOG_WARN("{}|failed to recycle env {}, get future error", traceID, envNameWithType);
                    }
                });
        }
    }
}

void VirtualEnvPluginMgrActor::RecycleUnusedEnvs()
{
    if (virtualEnvIdleTime_ <= -1) {
        YRLOG_INFO("No need to recycle envs");
        return;
    }
    DoClearVirtualEnv();
    recycleUnusedEnvsTimer_ =
        litebus::AsyncAfter(envRefRecycleInterval_, GetAID(), &VirtualEnvPluginMgrActor::RecycleUnusedEnvs);
}

void VirtualEnvPluginMgrActor::SetVirtualEnvIdleTime(const PluginInfo &pluginInfo)
{
    const auto virtualEnvIdleTimeIt = pluginInfo.extra_params.find(VIRTUAL_ENV_IDLE_TIME);
    if (virtualEnvIdleTimeIt != pluginInfo.extra_params.end()) {
        try {
            this->virtualEnvIdleTime_ = std::stoi(virtualEnvIdleTimeIt->second);
        } catch (...) {
            this->virtualEnvIdleTime_ = -1;
        }
    }
}
}  // namespace functionsystem::function_agent
