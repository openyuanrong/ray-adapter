/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef COMMON_CRYPTO_H
#define COMMON_CRYPTO_H

#include <async/option.hpp>

#include "common/status/status.h"
#include "common/utils/sensitive_value.h"
#include "common/utils/singleton.h"

namespace functionsystem {
class Crypto : public Singleton<Crypto> {
public:
    Crypto() = default;

    ~Crypto() override = default;

    void SetAlgorithm(const std::string &algorithm);

    Status LoadSecretKey(const std::string &resourcePath);

    litebus::Option<std::pair<std::string, std::string>> Encrypt(const SensitiveValue &plaintext);

    litebus::Option<SensitiveValue> Decrypt(const std::string &ciphertext, const std::string &key = "",
                                            const std::string &algorithm = "");

private:
    std::string algorithm_;
};
}  // namespace functionsystem

#endif  // COMMON_CRYPTO_H
