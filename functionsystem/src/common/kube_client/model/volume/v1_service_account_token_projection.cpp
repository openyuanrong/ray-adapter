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

#include "v1_service_account_token_projection.h"

namespace functionsystem::kube_client {
namespace model {
REGISTER_MODEL_OBJECT(V1ServiceAccountTokenProjection);

V1ServiceAccountTokenProjection::V1ServiceAccountTokenProjection()
    : m_audienceIsSet(false), m_expirationSeconds(0L), m_expirationSecondsIsSet(false), m_pathIsSet(false)
{
}

V1ServiceAccountTokenProjection::~V1ServiceAccountTokenProjection()
= default;

nlohmann::json V1ServiceAccountTokenProjection::ToJson() const
{
    nlohmann::json val = nlohmann::json::object();
    if (AudienceIsSet()) {
        val["audience"] = ModelUtils::ToJson(m_audience);
    }
    if (ExpirationSecondsIsSet()) {
        val["expirationSeconds"] = ModelUtils::ToJson(m_expirationSeconds);
    }
    if (PathIsSet()) {
        val["path"] = ModelUtils::ToJson(m_Path);
    }

    return val;
}

bool V1ServiceAccountTokenProjection::FromJson(const nlohmann::json &val)
{
    bool ok = true;

    if (val.contains("audience")) {
        const nlohmann::json &fieldValue = val.at("audience");
        if (!fieldValue.is_null()) {
            std::string refVal_setAudience;
            ok &= ModelUtils::FromJson(fieldValue, refVal_setAudience);
            SetAudience(refVal_setAudience);
        }
    }
    if (val.contains("expirationSeconds")) {
        const nlohmann::json &fieldValue = val.at("expirationSeconds");
        if (!fieldValue.is_null()) {
            int64_t refVal_setExpirationSeconds;
            ok &= ModelUtils::FromJson(fieldValue, refVal_setExpirationSeconds);
            SetExpirationSeconds(refVal_setExpirationSeconds);
        }
    }
    if (val.contains("path")) {
        const nlohmann::json &fieldValue = val.at("path");
        if (!fieldValue.is_null()) {
            std::string refVal_setPath;
            ok &= ModelUtils::FromJson(fieldValue, refVal_setPath);
            SetPath(refVal_setPath);
        }
    }
    return ok;
}

std::string V1ServiceAccountTokenProjection::GetAudience() const
{
    return m_audience;
}

void V1ServiceAccountTokenProjection::SetAudience(const std::string &value)
{
    m_audience = value;
    m_audienceIsSet = true;
}

bool V1ServiceAccountTokenProjection::AudienceIsSet() const
{
    return m_audienceIsSet;
}

void V1ServiceAccountTokenProjection::UnsetAudience()
{
    m_audienceIsSet = false;
}

int64_t V1ServiceAccountTokenProjection::GetExpirationSeconds() const
{
    return m_expirationSeconds
}

void V1ServiceAccountTokenProjection::SetExpirationSeconds(int64_t value)
{
    m_expirationSeconds = value;
    m_expirationSecondsIsSet = true;
}

bool V1ServiceAccountTokenProjection::ExpirationSecondsIsSet() const
{
    return m_expirationSecondsIsSet;
}

void V1ServiceAccountTokenProjection::UnsetExpirationSeconds()
{
    m_expirationSecondsIsSet = false;
}

std::string V1ServiceAccountTokenProjection::GetPath() const
{
    return m_path;
}

void V1ServiceAccountTokenProjection::SetPath(const std::string &value)
{
    m_path = value;
    m_pathIsSet = true;
}

bool V1ServiceAccountTokenProjection::PathIsSet() const
{
    return m_pathIsSet;
}

void V1ServiceAccountTokenProjection::UnsetPath()
{
    m_pathIsSet = false;
}
}  // namespace model
}  // namespace functionsystem::kube_client