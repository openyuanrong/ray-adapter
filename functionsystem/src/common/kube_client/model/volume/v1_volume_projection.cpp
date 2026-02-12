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

#include "v1_volume_projection.h"

namespace functionsystem::kube_client {
namespace model {

REGISTER_MODEL_OBJECT(V1VolumeProjection);

V1VolumeProjection::V1VolumeProjection()
{
    m_downwardAPIIsSet = false;
    m_serviceAccountTokenIsSet = false;
}

V1VolumeProjection::~V1VolumeProjection()
{
}

nlohmann::json V1VolumeProjection::ToJson() const
{
    nlohmann::json val = nlohmann::json::object();

    if (m_downwardAPIIsSet) {
        val["downwardAPI"] = ModelUtils::ToJson(m_downwardAPI);
    }
    if (m_serviceAccountTokenIsSet) {
        val["serviceAccountToken"] = ModelUtils::ToJson(m_ServiceAccountToken);
    }

    return val;
}

bool V1VolumeProjection::FromJson(const nlohmann::json &val)
{
    bool ok = true;

    if (val.contains("downwardAPI")) {
        const nlohmann::json &fieldValue = val.at("downwardAPI");
        if (!fieldValue.is_null()) {
            std::shared_ptr<V1DownwardAPIProjection> refValSetDownwardAPI;
            ok &= ModelUtils::FromJson(fieldValue, refValSetDownwardAPI);
            SetDownwardAPI(refValSetDownwardAPI);
        }
    }
    if (val.contains("serviceAccountToken")) {
        const nlohmann::json &fieldValue = val.at("serviceAccountToken");
        if (!fieldValue.is_null()) {
            std::shared_ptr<V1ServiceAccountTokenProjection> refVal_setServiceAccountToken;
            ok &= ModelUtils::FromJson(fieldValue, refVal_setServiceAccountToken);
            SetServiceAccountToken(refVal_setServiceAccountToken);
        }
    }
    return ok;
}

std::shared_ptr<V1DownwardAPIProjection> V1VolumeProjection::GetDownwardAPI() const
{
    return m_downwardAPI;
}

void V1VolumeProjection::SetDownwardAPI(const std::shared_ptr<V1DownwardAPIProjection> &value)
{
    m_downwardAPI = value;
    m_downwardAPIIsSet = true;
}

bool V1VolumeProjection::DownwardAPIIsSet() const
{
    return m_downwardAPIIsSet;
}

void V1VolumeProjection::UnsetDownwardAPI()
{
    m_downwardAPIIsSet = false;
}

std::shared_ptr<V1ServiceAccountTokenProjection> V1VolumeProjection::GetServiceAccountToken() const
{
    return m_ServiceAccountToken;
}

void V1VolumeProjection::SetServiceAccountToken(const std::shared_ptr<V1ServiceAccountTokenProjection> &value)
{
    m_ServiceAccountToken = value;
    m_serviceAccountTokenIsSet = true;
}

bool V1VolumeProjection::ServiceAccountTokenIsSet() const
{
    return m_serviceAccountTokenIsSet;
}

void V1VolumeProjection::UnsetServiceAccountToken()
{
    m_serviceAccountTokenIsSet = false;
}

}  // namespace model
}  // namespace functionsystem::kube_client
