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

#include "resource_group_manager.h"
#include "resource_group_manager_actor.h"

namespace functionsystem::resource_group_manager {

void ResourceGroupManager::OnDeleteInstance(const std::shared_ptr<resource_view::InstanceInfo> &ins)
{
    ASSERT_IF_NULL(actor_);
    return litebus::Async(actor_->GetAID(), &ResourceGroupManagerActor::OnDeleteInstance, ins);
}

void ResourceGroupManager::OnKillJob(const std::string &jobId)
{
    ASSERT_IF_NULL(actor_);
    return litebus::Async(actor_->GetAID(), &ResourceGroupManagerActor::OnKillJob, jobId);
}
} // namespace functionsystem::resource_group_manager
