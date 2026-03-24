#!/usr/bin/env python3
# coding=UTF-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


__all__ = [
    "init", "_make_remote", "remote", "get_actor", "nodes", "available_resources",
    "cluster_resources", "get", "finalize", "ExistenceOpt",
    "WriteMode", "CacheType", "SetParam", "MSetParam", "CreateParam",
    "AlarmSeverity", "AlarmInfo", "ConsistencyType", "GetParams", "GetParam", "put",
    "get_runtime_context", "ObjectRef", "GetTimeoutError", "RayTaskError",
    "JobSubmissionClient", "cancel", "cloudpickle"
]

import sys

from yr.object_ref import ObjectRef
from ray_adapter.worker import (
    _make_remote, remote, get_actor, nodes, available_resources, cluster_resources, get,
    is_initialized, shutdown, available_resources_per_node, method, kill, init, wait, cancel

)
from ray_adapter import util
from ray_adapter import actor
from ray_adapter.job_submission.sdk import JobSubmissionClient
from ray_adapter.runtime_context import get_runtime_context
from ray_adapter.exceptions import GetTimeoutError, RayTaskError
from ray_adapter._private import state
from ray_adapter import cloudpickle

from yr.apis import (
    finalize, put, resources
)
