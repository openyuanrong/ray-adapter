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

#ifndef COMMON_UTILS_AKSK_CONTENT_H
#define COMMON_UTILS_AKSK_CONTENT_H

#include <nlohmann/json.hpp>

#include "common/status/status.h"
#include "common/utils/sensitive_value.h"

namespace functionsystem {

const std::string TENANT_ID_STR = "tenantID";
const std::string ACCESS_KEY_STR = "accessKey";
const std::string SECRET_KEY_STR = "secretKey";
const std::string DATA_KEY_STR = "dataKey";
const std::string EXPIRED_TIME_STAMP_STR = "expiredTimeStamp";
const std::string EXPIRED_TIME_SPAN_STR = "expiredTimeSpan";
const std::string CREDENTIAL_NAME_KEY_STR = "credentialName";
const std::string SERVICE_NAME_KEY_STR = "serviceName";
const std::string MICROSERVICE_NAMES_KEY_STR = "microserviceNames";
const uint32_t DATA_KEY_IV_LEN = 12;
const std::string SPLIT_SYMBOL = "_";
// if credential will be expired in $offset seconds, we need to generate new right now
const uint32_t NEW_CREDENTIAL_EXPIRED_OFFSET = 30;
// tenant roles definition
const std::string ROLE_STR = "role";
const std::string SYSTEM_ROLE = "system";
const std::string NORMAL_ROLE = "normal";
const std::string UNCONFIRMED_ROLE = "unconfirmed";

struct AKSKContent {
    std::string tenantID;
    std::string accessKey;
    SensitiveValue secretKey;
    SensitiveValue dataKey;
    uint64_t expiredTimeStamp{ 0 };
    std::string role{ UNCONFIRMED_ROLE };

    Status status{ Status::OK() };

    Status IsValid(const uint32_t offset = 0)
    {
        if (status.IsError()) {
            return status;
        }
        if (tenantID.empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "aksk tenantID is empty");
        }
        if (accessKey.empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "aksk accessKey is empty");
        }
        if (secretKey.Empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "aksk secretKey is empty");
        }
        if (dataKey.Empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "aksk dataKey is empty");
        }
        auto now = static_cast<uint64_t>(std::time(nullptr));
        if (expiredTimeStamp < now + offset && expiredTimeStamp > 0) {
            return Status(StatusCode::PARAMETER_ERROR, "aksk expired time stamp is earlier than now, expiredTimeStamp: "
                                                           + std::to_string(expiredTimeStamp));
        }
        return status;
    }

    std::shared_ptr<AKSKContent> Copy()
    {
        auto akskContent = std::make_shared<AKSKContent>();
        akskContent->tenantID = tenantID;
        akskContent->accessKey = accessKey;
        akskContent->secretKey = secretKey;
        akskContent->dataKey = dataKey;
        akskContent->expiredTimeStamp = expiredTimeStamp;
        akskContent->role = role;
        return akskContent;
    }
};

struct EncAKSKContent {
    std::string tenantID;
    std::string accessKey;
    std::string secretKey;
    std::string dataKey;
    uint64_t expiredTimeStamp;
    // only for datasystem to refresh cache in case for clock inconsistency
    uint64_t expiredTimeSpan{ 0 };
    std::string role{ UNCONFIRMED_ROLE };

    Status status{ Status::OK() };

    Status IsValid(const uint32_t offset = 0)
    {
        if (status.IsError()) {
            return status;
        }
        if (tenantID.empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "encrypt aksk tenantID is empty");
        }
        if (accessKey.empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "encrypt aksk tenantID is empty");
        }
        if (secretKey.empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "encrypt aksk secretKey is empty");
        }
        if (dataKey.empty()) {
            return Status(StatusCode::PARAMETER_ERROR, "encrypt aksk dataKey is empty");
        }
        auto now = static_cast<uint64_t>(std::time(nullptr));
        if (expiredTimeStamp < now + offset && expiredTimeStamp > 0) {
            return Status(StatusCode::PARAMETER_ERROR,
                          "encrypt aksk expired time stamp is earlier than now, expiredTimeStamp: "
                              + std::to_string(expiredTimeStamp));
        }
        return status;
    }
};

struct PermanentCredential {
    std::string tenantID;
    // if sts is enabled, will save credential to sts, get credential from sts
    std::string credentialName;
    std::string serviceName;
    std::vector<std::string> microserviceNames;
};

Status EncryptKeyToString(const SensitiveValue &key, std::string &encryptedKey);

Status DecryptKeyFromString(SensitiveValue &key, const std::string &encryptedKey);

std::shared_ptr<EncAKSKContent> TransToEncAKSKContentFromJson(const std::string &json);

std::shared_ptr<AKSKContent> TransToAKSKContentFromJson(const std::string &json);

std::shared_ptr<AKSKContent> TransToAKSKContentFromJsonNew(const std::string &json);

std::string TransToJsonFromAKSKContent(const std::shared_ptr<AKSKContent> &akskContent);

std::string TransToJsonFromEncAKSKContent(const std::shared_ptr<EncAKSKContent> &encAKSKContent);

std::vector<std::shared_ptr<PermanentCredential>> TransToPermanentCredFromJson(const std::string &confJson);

// decrypt aksk with root key & work key
std::shared_ptr<AKSKContent> DecryptAKSKContentFromStorage(const std::shared_ptr<EncAKSKContent> &encAKSKContent);

// decrypt aksk with root key & work key
std::shared_ptr<EncAKSKContent> EncryptAKSKContentForStorage(const std::shared_ptr<AKSKContent> &akskContent);
}  // namespace functionsystem
#endif  // COMMON_UTILS_AKSK_CONTENT_H
