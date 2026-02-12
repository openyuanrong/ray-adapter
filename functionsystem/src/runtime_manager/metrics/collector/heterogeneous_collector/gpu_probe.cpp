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
#include "gpu_probe.h"

#include "common/logs/logging.h"
#include "common/utils/exec_utils.h"
#include "utils/utils.h"
#include "partitioner.h"

namespace functionsystem::runtime_manager {
const static std::string GET_GPU_TOPO_INFO_CMD = "nvidia-smi topo -m";
const static std::string GET_GPU_INFO_CMD = "nvidia-smi";
const static std::string QUERY_GPU_OR_UNIT_INFO_CMD = "nvidia-smi --query-gpu=name --format=csv,noheader";
const static std::string QUERY_GPU_ID_CMD = "nvidia-smi --query-gpu=index --format=csv,noheader";
const static int BASE_TYPE_NUM = 7;
const static int GPU_ROW_INTERVAL = 4;
const static int MEMORY_KEY_INDEX = 5;
const static int USED_MEMORY_VAL_INDEX = 8;
const static int TOTAL_MEMORY_VAL_INDEX = 10;
const static int MIB_LENGTH = 3;

GpuProbe::GpuProbe(const std::string &ldLibraryPath, std::shared_ptr<CmdTool> cmdTool) : TopoProbe(cmdTool)
{
    devInfo_ = std::make_shared<DevCluster>();
    devInfo_->devType = DEV_TYPE_GPU;
    devInfo_->devVendor = DEV_VENDOR_NVIDIA;
    AddLdLibraryPathForGpuCmd(ldLibraryPath);
    ExtractVisibleDevicesFromEnvVar(CUDA_VISIBLE_DEVICES_ENV_VAR);
}

void GpuProbe::UpdateTopoPartition()
{
    if (devInfo_->devTopo.empty()) {
        YRLOG_WARN("devTopo info is wrong");
        return;
    }
    auto totalslots = pow(2, ceil(log2((double(devInfo_->devTopo.size())))));
    devInfo_->devPartition.resize(size_t(totalslots));
    Partitioner partitioner = Partitioner();
    auto partitionInfo = partitioner.GetPartition(ConvertPartition(devInfo_->devTopo));
    size_t idx = 0;
    for (auto partition : partitionInfo) {
        if (partition == -1) {
            devInfo_->devPartition[idx] = "null";
        } else {
            devInfo_->devPartition[idx] = devInfo_->devIDs[static_cast<unsigned short>(partition)];
        }
        idx++;
    }
}

void GpuProbe::UpdateDevTopo()
{
    std::vector<std::string> topoResult = cmdTool_->GetCmdResult(getGpuTopoInfoCmd);
    if (topoResult.empty()) {
        std::string msg = "The node does not install gpu driver";
        YRLOG_ERROR(msg);
        return;
    }
    devInfo_->devTopo = GetTopoInfo(topoResult, detectedDeviceCnt_);
}

void GpuProbe::UpdateHBM()
{
    std::vector<std::string> hbmResult = cmdTool_->GetCmdResult(getGpuInfoCmd);
    if (hbmResult.empty() || hbmResult.size() < BASE_TYPE_NUM + detectedDeviceCnt_ * GPU_ROW_INTERVAL) {
        YRLOG_ERROR("using {} to get hbm is wrong", GET_GPU_INFO_CMD);
        return;
    }
    std::vector<std::string> columns = GetColumnValue(hbmResult[BASE_TYPE_NUM - 1 - 1]);
    if (columns.size() < MEMORY_KEY_INDEX + 1 || (columns[MEMORY_KEY_INDEX].find("Memory-Usage") == std::string::npos
        && columns[MEMORY_KEY_INDEX + 1].find("Memory-Usage") == std::string::npos)) {
        YRLOG_WARN("cannot use {} to get hbm, set default value 16384Mb", GET_GPU_INFO_CMD);
        (void)devInfo_->devLimitHBMs.insert(devInfo_->devUsedHBM.begin(), detectedDeviceCnt_, 0);
        return;
    }
    for (size_t i = 0; i < detectedDeviceCnt_; i++) {
        std::vector<std::string> hbms = GetColumnValue(hbmResult[BASE_TYPE_NUM - 1 + GPU_ROW_INTERVAL * (i + 1) - 1]);
        if (hbms.size() < TOTAL_MEMORY_VAL_INDEX + 1) {
            YRLOG_ERROR("failed to get hbm value");
            return;
        }
        try {
            devInfo_->devLimitHBMs.push_back(
                std::stoi(hbms[TOTAL_MEMORY_VAL_INDEX].substr(0, hbms[TOTAL_MEMORY_VAL_INDEX].size() - MIB_LENGTH)));
            devInfo_->devUsedHBM.push_back(
                std::stoi(hbms[USED_MEMORY_VAL_INDEX].substr(0, hbms[USED_MEMORY_VAL_INDEX].size() - MIB_LENGTH)));
        } catch (const std::exception &e) {
            YRLOG_WARN("stoi fail, error:{}", e.what());
        }
    }
}

void GpuProbe::UpdateMemory()
{
    if (detectedDeviceCnt_ != 0) {
        devInfo_->devTotalMemory.insert(devInfo_->devTotalMemory.begin(), detectedDeviceCnt_, 0);
    }
}

void GpuProbe::UpdateUsedMemory()
{
    if (detectedDeviceCnt_ != 0) {
        devInfo_->devUsedMemory.insert(devInfo_->devUsedMemory.begin(), detectedDeviceCnt_, 0);
    }
}

void GpuProbe::UpdateHealth()
{
    devInfo_->health.clear();
    // stub health ok
    for (size_t i = 0; i < detectedDeviceCnt_; ++i) {
        devInfo_->health.push_back(0);
    }
}

Status GpuProbe::RefreshTopo()
{
    if (init) {
        return Status(StatusCode::SUCCESS);
    }
    init = true;
    if (auto status = HasGpu(); status.IsError()) {
        return status;
    }

    UpdateProductModel();
    UpdateDevTopo();
    UpdateTopoPartition();
    UpdateHBM();
    UpdateMemory();
    UpdateUsedMemory();
    UpdateUsedHBM();
    UpdateHealth();
    FilterDevicesEnvVar();

    return Status(StatusCode::SUCCESS);
}

void GpuProbe::AddLdLibraryPathForGpuCmd(const std::string &ldLibraryPath)
{
    getGpuTopoInfoCmd = Utils::LinkCommandWithLdLibraryPath(ldLibraryPath, GET_GPU_TOPO_INFO_CMD);
    getGpuInfoCmd = Utils::LinkCommandWithLdLibraryPath(ldLibraryPath, GET_GPU_INFO_CMD);
    queryGpuOrUnitInfoCmd_ = Utils::LinkCommandWithLdLibraryPath(ldLibraryPath, QUERY_GPU_OR_UNIT_INFO_CMD);
    queryGpuIdCmd_ = Utils::LinkCommandWithLdLibraryPath(ldLibraryPath, QUERY_GPU_ID_CMD);
}

void GpuProbe::UpdateProductModel()
{
    std::vector<std::string> queryResult = cmdTool_->GetCmdResult(queryGpuOrUnitInfoCmd_);
    if (queryResult.empty()) {
        YRLOG_ERROR("using {} to query gpu or unit info failed.", QUERY_GPU_OR_UNIT_INFO_CMD);
        return;
    }
    devInfo_->devProductModel = litebus::strings::Trim(queryResult[0]);
}

Status GpuProbe::HasGpu()
{
    std::vector<std::string> res = cmdTool_->GetCmdResult(queryGpuIdCmd_);
    if (res.empty()) {
        YRLOG_ERROR("The node does not have gpu device");
        return Status(StatusCode::RUNTIME_MANAGER_GPU_NOTFOUND, "The node does not have gpu device");
    }
    std::vector<int> ids;
    try {
        for (const auto &id : res) {
            ids.push_back(std::stoi(id));
        }
    } catch (const std::exception &e) {
        YRLOG_ERROR("Illegal gpu id, error:{}", e.what());
        return Status(StatusCode::RUNTIME_MANAGER_GPU_NOTFOUND, "Illegal gpu id");
    }
    devInfo_->devIDs = std::move(ids);
    detectedDeviceCnt_ = devInfo_->devIDs.size();
    hasXPU_ = true;
    return Status(StatusCode::SUCCESS);
}
}