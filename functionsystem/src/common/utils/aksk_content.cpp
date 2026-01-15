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

#include "aksk_content.h"

#include <nlohmann/json.hpp>

#include "common/crypto/crypto.h"
#include "common/hex/hex.h"
#include "common/logs/logging.h"
#include "common/status/status.h"
#include "common/utils/sensitive_value.h"

namespace functionsystem {
Status EncryptKeyToString(const SensitiveValue &key, std::string &encryptedKey)
{
    const auto ciphertext = Crypto::GetInstance().Encrypt(key);
    if (ciphertext.IsNone()) {
        return Status(StatusCode::PARAMETER_ERROR, "failed to encrypt key");
    }

    encryptedKey = ciphertext.Get().first + SPLIT_SYMBOL + ciphertext.Get().second;
    return Status::OK();
}

Status DecryptKeyFromString(SensitiveValue &key, const std::string &encryptedKey)
{
    auto pos = encryptedKey.find(SPLIT_SYMBOL);
    if (pos == std::string::npos) {
        return Status(StatusCode::PARAMETER_ERROR, "not found split symbol");
    }

    auto plaintext = Crypto::GetInstance().Decrypt(encryptedKey.substr(pos + 1), encryptedKey.substr(0, pos));
    if (plaintext.IsNone()) {
        return Status(StatusCode::PARAMETER_ERROR, "failed to decrypt key");
    }

    key = plaintext.Get();
    return Status::OK();
}

std::shared_ptr<EncAKSKContent> TransToEncAKSKContentFromJson(const std::string &json)
{
    auto encAKSKContent = std::make_shared<EncAKSKContent>();
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (std::exception &error) {
        YRLOG_WARN("failed to parse encrypt aksk content, error: {}", error.what());
        encAKSKContent->status =
            Status(StatusCode::PARAMETER_ERROR, "parse json failed, err: " + std::string(error.what()));
        return encAKSKContent;
    }
    if (j.find(TENANT_ID_STR) != j.end()) {
        encAKSKContent->tenantID = j.at(TENANT_ID_STR).get<std::string>();
    }
    if (j.find(ACCESS_KEY_STR) != j.end()) {
        encAKSKContent->accessKey = j.at(ACCESS_KEY_STR).get<std::string>();
    }
    if (j.find(SECRET_KEY_STR) != j.end()) {
        encAKSKContent->secretKey = j.at(SECRET_KEY_STR).get<std::string>();
    }
    if (j.find(DATA_KEY_STR) != j.end()) {
        encAKSKContent->dataKey = j.at(DATA_KEY_STR).get<std::string>();
    }
    if (j.find(EXPIRED_TIME_STAMP_STR) != j.end()) {
        std::string expiredTimeStampStr = j.at(EXPIRED_TIME_STAMP_STR).get<std::string>();
        try {
            encAKSKContent->expiredTimeStamp = std::stoull(expiredTimeStampStr);
        } catch (std::exception &e) {
            YRLOG_WARN("failed to parse expiredTimeStamp, error: {}", e.what());
            encAKSKContent->status =
                Status(StatusCode::PARAMETER_ERROR, "parse expiredTimeStamp failed, err: " + std::string(e.what()));
            return encAKSKContent;
        }
    }
    if (j.find(EXPIRED_TIME_SPAN_STR) != j.end()) {
        std::string expiredTimeSpanStr = j.at(EXPIRED_TIME_SPAN_STR).get<std::string>();
        try {
            encAKSKContent->expiredTimeSpan = std::stoull(expiredTimeSpanStr);
        } catch (std::exception &e) {
            YRLOG_WARN("failed to parse expiredTimeSpan, error: {}", e.what());
            encAKSKContent->status =
                Status(StatusCode::PARAMETER_ERROR, "parse expiredTimeSpan failed, err: " + std::string(e.what()));
            return encAKSKContent;
        }
    }
    if (j.find(ROLE_STR) != j.end()) {
        encAKSKContent->role = j.at(ROLE_STR).get<std::string>();
    }
    return encAKSKContent;
}

std::shared_ptr<AKSKContent> TransToAKSKContentFromJson(const std::string &json)
{
    auto akskContent = std::make_shared<AKSKContent>();
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (std::exception &error) {
        YRLOG_WARN("failed to parse plain aksk content, error: {}", error.what());
        akskContent->status =
            Status(StatusCode::PARAMETER_ERROR, "parse json failed, err: " + std::string(error.what()));
        return akskContent;
    }
    if (j.find(TENANT_ID_STR) != j.end()) {
        akskContent->tenantID = j.at(TENANT_ID_STR).get<std::string>();
    }
    if (j.find(ACCESS_KEY_STR) != j.end()) {
        akskContent->accessKey = j.at(ACCESS_KEY_STR).get<std::string>();
    }
    if (j.find(SECRET_KEY_STR) != j.end()) {
        akskContent->secretKey = HexStringToCharString(j.at(SECRET_KEY_STR).get<std::string>());
    }
    if (j.find(DATA_KEY_STR) != j.end()) {
        akskContent->dataKey = HexStringToCharString(j.at(DATA_KEY_STR).get<std::string>());
    }
    akskContent->expiredTimeStamp = 0;
    if (j.find(ROLE_STR) != j.end()) {
        akskContent->role = j.at(ROLE_STR).get<std::string>();
    }
    return akskContent;
}

std::shared_ptr<AKSKContent> TransToAKSKContentFromJsonNew(const std::string &json)
{
    auto content = std::make_shared<AKSKContent>();
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (std::exception &error) {
        YRLOG_WARN("failed to parse plain aksk content, error: {}", error.what());
        content->status = Status(StatusCode::PARAMETER_ERROR, "parse json failed, err: " + std::string(error.what()));
        return content;
    }
    if (j.find(ACCESS_KEY_STR) != j.end()) {
        content->accessKey = j.at(ACCESS_KEY_STR).get<std::string>();
    }
    if (j.find(SECRET_KEY_STR) != j.end()) {
        content->secretKey = HexStringToCharString(j.at(SECRET_KEY_STR).get<std::string>());
    }
    if (j.find(DATA_KEY_STR) != j.end()) {
        content->dataKey = HexStringToCharString(j.at(DATA_KEY_STR).get<std::string>());
    }
    return content;
}

std::string TransToJsonFromAKSKContent(const std::shared_ptr<AKSKContent> &akskContent)
{
    nlohmann::json val = nlohmann::json::object();
    val[TENANT_ID_STR] = akskContent->tenantID;
    val[ACCESS_KEY_STR] = akskContent->accessKey;
    val[SECRET_KEY_STR] =
        CharStringToHexString(std::string(akskContent->secretKey.GetData(), akskContent->secretKey.GetSize()));
    val[DATA_KEY_STR] =
        CharStringToHexString(std::string(akskContent->dataKey.GetData(), akskContent->dataKey.GetSize()));
    return val.dump();
}

std::string TransToJsonFromEncAKSKContent(const std::shared_ptr<EncAKSKContent> &encAKSKContent)
{
    nlohmann::json val = nlohmann::json::object();
    val[TENANT_ID_STR] = encAKSKContent->tenantID;
    val[ACCESS_KEY_STR] = encAKSKContent->accessKey;
    val[SECRET_KEY_STR] = encAKSKContent->secretKey;
    val[DATA_KEY_STR] = encAKSKContent->dataKey;
    val[EXPIRED_TIME_STAMP_STR] = std::to_string(encAKSKContent->expiredTimeStamp);
    val[EXPIRED_TIME_SPAN_STR] = std::to_string(encAKSKContent->expiredTimeSpan);
    val[ROLE_STR] = encAKSKContent->role;
    return val.dump();
}

std::vector<std::shared_ptr<PermanentCredential>> TransToPermanentCredFromJson(const std::string &confJson)
{
    if (confJson.empty()) {
        return {};
    }

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(confJson);
    } catch (std::exception &error) {
        YRLOG_ERROR("parse json failed, error: {}", error.what());
        return {};
    }
    if (!json.is_array()) {
        YRLOG_ERROR("config is not an array");
        return {};
    }
    std::vector<std::shared_ptr<PermanentCredential>> permanentCreds{};
    for (auto &j : json) {
        auto credential = std::make_shared<PermanentCredential>();
        if (j.find(TENANT_ID_STR) != j.end()) {
            credential->tenantID = std::move(j.at(TENANT_ID_STR).get<std::string>());
        }
        if (credential->tenantID.empty()) {
            YRLOG_WARN("tenantID is empty, skip it");
            continue;
        }
        if (j.find(CREDENTIAL_NAME_KEY_STR) != j.end()) {
            credential->credentialName = std::move(j.at(CREDENTIAL_NAME_KEY_STR).get<std::string>());
        }
        if (j.find(SERVICE_NAME_KEY_STR) != j.end()) {
            credential->serviceName = std::move(j.at(SERVICE_NAME_KEY_STR).get<std::string>());
        }
        if (j.find(MICROSERVICE_NAMES_KEY_STR) != j.end()) {
            for (const auto &microserviceName : j[MICROSERVICE_NAMES_KEY_STR]) {
                credential->microserviceNames.push_back(microserviceName);
            }
        }
        permanentCreds.emplace_back(credential);
    }
    return permanentCreds;
}

// decrypt aksk with root key & work key
std::shared_ptr<AKSKContent> DecryptAKSKContentFromStorage(const std::shared_ptr<EncAKSKContent> &encAKSKContent)
{
    auto akskContent = std::make_shared<AKSKContent>();
    akskContent->tenantID = encAKSKContent->tenantID;
    akskContent->accessKey = encAKSKContent->accessKey;
    akskContent->expiredTimeStamp = encAKSKContent->expiredTimeStamp;
    akskContent->role = encAKSKContent->role;
    if (const auto status(DecryptKeyFromString(akskContent->dataKey, encAKSKContent->dataKey)); status.IsError()) {
        akskContent->status = Status(PARAMETER_ERROR, "decrypt dataKey failed.");
        YRLOG_ERROR("decrypt dataKey failed: {}", status.ToString());
        return akskContent;
    }
    if (const auto status(DecryptKeyFromString(akskContent->secretKey, encAKSKContent->secretKey)); status.IsError()) {
        akskContent->status = Status(PARAMETER_ERROR, "decrypt secretKey failed.");
        YRLOG_ERROR("decrypt secretKey failed: {}", status.ToString());
        return akskContent;
    }
    return akskContent;
}

// decrypt aksk with root key & work key
std::shared_ptr<EncAKSKContent> EncryptAKSKContentForStorage(const std::shared_ptr<AKSKContent> &akskContent)
{
    auto encAKSKContent = std::make_shared<EncAKSKContent>();
    encAKSKContent->tenantID = akskContent->tenantID;
    encAKSKContent->accessKey = akskContent->accessKey;
    encAKSKContent->expiredTimeStamp = akskContent->expiredTimeStamp;
    encAKSKContent->role = akskContent->role;
    if (const auto status(EncryptKeyToString(akskContent->dataKey, encAKSKContent->dataKey)); status.IsError()) {
        encAKSKContent->status = Status(PARAMETER_ERROR, "encrypt dataKey failed.");
        YRLOG_ERROR("encrypt dataKey failed: {}", status.ToString());
        return encAKSKContent;
    }
    if (const auto status(EncryptKeyToString(akskContent->secretKey, encAKSKContent->secretKey)); status.IsError()) {
        encAKSKContent->status = Status(PARAMETER_ERROR, "encrypt secretKey failed.");
        YRLOG_ERROR("encrypt secretKey failed: {}", status.ToString());
        return encAKSKContent;
    }
    return encAKSKContent;
}

}  // namespace functionsystem
