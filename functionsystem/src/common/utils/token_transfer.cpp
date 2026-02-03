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

#include "token_transfer.h"

#include <securec.h>

#include <nlohmann/json.hpp>

#include "common/crypto/crypto.h"
#include "common/logs/logging.h"

namespace functionsystem {
std::string TransToJsonFromTokenSalt(const TokenSalt &tokenSalt)
{
    nlohmann::json val = nlohmann::json::object();
    val[TOKEN_STR] = tokenSalt.token;
    val[SALT_STR] = tokenSalt.salt;
    return val.dump();
}

std::shared_ptr<TokenSalt> TransTokenSaltFromJson(const std::string &json)
{
    auto tokenSalt = std::make_shared<TokenSalt>();
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json);
    } catch (std::exception &error) {
        YRLOG_WARN("failed to parse token and salt, error: {}", error.what());
        return tokenSalt;
    }
    if (j.find(TOKEN_STR) != j.end()) {
        tokenSalt->token = j.at(TOKEN_STR).get<std::string>();
    }
    if (j.find(SALT_STR) != j.end()) {
        tokenSalt->salt = j.at(SALT_STR).get<std::string>();
    }
    return tokenSalt;
}

void CleanSensitiveStrMemory(std::string &sensitiveStr, const std::string &msg)
{
    auto ret = memset_s(&sensitiveStr[0], sensitiveStr.length(), 0, sensitiveStr.length());
    if (ret != 0) {
        YRLOG_WARN("token clean memory failed when {}.", msg);
    }
}

Status DecryptTokenSaltFromStorage(const std::string &tokenStrFromStorage, const std::shared_ptr<TokenSalt> &tokenSalt)
{
    auto pos = tokenStrFromStorage.find('_');
    if (pos == std::string::npos) {
        return Status(StatusCode::FAILED, "decrypt token from storage error!");
    }
    std::string key = tokenStrFromStorage.substr(0, pos);
    std::string info = tokenStrFromStorage.substr(pos + 1);
    auto decryptTokenFromStorage = Crypto::GetInstance().Decrypt(info, key);
    if (decryptTokenFromStorage.IsNone()) {
        return Status(StatusCode::FAILED, "proxy iam decrypt token from storage error!");
    }
    std::string tokenStr(decryptTokenFromStorage.Get().GetData());
    // token in storage format is: Encrypt(encryptToken+timestamp)
    pos = tokenStr.rfind(SPLIT_SYMBOL_TOKEN);
    if (pos == std::string::npos) {
        return Status(StatusCode::FAILED, "get token from decrypt storage token failed!");
    }
    tokenSalt->token = tokenStr.substr(0, pos);
    try {
        std::string expiredTimeStr = tokenStr.substr(pos + 1);
        tokenSalt->expiredTimeStamp = std::stoull(expiredTimeStr);
    } catch (std::exception &e) {
        return Status(StatusCode::FAILED, "transform time stamp type failed, err:" + std::string(e.what()));
    }
    CleanSensitiveStrMemory(tokenStr, "decrypt token from storage");
    return Status::OK();
}
}  // namespace functionsystem
