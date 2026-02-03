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

#include "crypto.h"

namespace functionsystem {
void Crypto::SetAlgorithm(const std::string &algorithm)
{
    // OpenYuanRong does not provide encryption and decryption algorithms by default. You can implement key management
    // and corresponding encryption/decryption algorithms according to your business requirements.
    algorithm_ = algorithm;
}

Status Crypto::LoadSecretKey(const std::string &)
{
    // OpenYuanRong does not provide encryption and decryption algorithms by default. You can implement key management
    // and corresponding encryption/decryption algorithms according to your business requirements.
    return Status::OK();
}

litebus::Option<std::pair<std::string, std::string>> Crypto::Encrypt(const SensitiveValue &ciphertext)
{
    // OpenYuanRong does not provide encryption and decryption algorithms by default. You can implement key management
    // and corresponding encryption/decryption algorithms according to your business requirements.
    return std::make_pair(std::string(), std::string(ciphertext.GetData()));
}

litebus::Option<SensitiveValue> Crypto::Decrypt(const std::string &plaintext, const std::string &, const std::string &)
{
    // OpenYuanRong does not provide encryption and decryption algorithms by default. You can implement key management
    // and corresponding encryption/decryption algorithms according to your business requirements.
    return SensitiveValue(plaintext);
}
}  // namespace functionsystem