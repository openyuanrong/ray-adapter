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

#ifndef FUNCTION_MASTER_RESOURCE_GROUP_MANAGER_H
#define FUNCTION_MASTER_RESOURCE_GROUP_MANAGER_H

#include "async/future.hpp"
#include "common/resource_view/resource_type.h"

namespace functionsystem::resource_group_manager {
class ResourceGroupManager {
public:
    explicit ResourceGroupManager(const litebus::ActorReference &actor) : actor_(actor) {};
    virtual ~ResourceGroupManager() = default;

    virtual void OnDeleteInstance(const std::shared_ptr<resource_view::InstanceInfo> &ins);
    virtual void OnKillJob(const std::string &jobId);

private:
    litebus::ActorReference actor_{ nullptr };
}; // class ResourceGroupManager
} // namespace functionsystem::resource_group_manager

#endif // FUNCTION_MASTER_RESOURCE_GROUP_MANAGER_H
