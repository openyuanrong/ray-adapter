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

#ifndef COMMON_UTILS_TOKEN_TRANSFER_H
#define COMMON_UTILS_TOKEN_TRANSFER_H

#include "common/status/status.h"

namespace functionsystem {
const std::string TOKEN_STR = "token";
const std::string SALT_STR = "salt";
const std::string SPLIT_SYMBOL_TOKEN = "+";
// if token will be expired in $offset seconds, we need to generate new right now
const uint32_t NEW_TOKEN_EXPIRED_OFFSET = 30;

struct TokenSalt {
    std::string token;
    std::string salt;
    uint64_t expiredTimeStamp{ 0 };
    Status status = Status::OK();
};

std::string TransToJsonFromTokenSalt(const TokenSalt &tokenSalt);

std::shared_ptr<TokenSalt> TransTokenSaltFromJson(const std::string &json);

void CleanSensitiveStrMemory(std::string &sensitiveStr, const std::string &msg);

Status DecryptTokenSaltFromStorage(const std::string &tokenStrFromStorage, const std::shared_ptr<TokenSalt> &tokenSalt);
}  // namespace functionsystem

#endif  // COMMON_UTILS_TOKEN_TRANSFER_H
