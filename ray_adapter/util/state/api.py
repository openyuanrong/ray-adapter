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
from typing import List, Dict
from ray_adapter.worker import nodes


def list_nodes() -> List[Dict]:
    """
    Retrieve the list of nodes.
    """

    node_infos = nodes()
    result = []

    for i, node in enumerate(node_infos):
        result.append({
            "node_id": node.get("NodeID", ""),
            "is_head_node": i == 0,
            "node_state": "ALIVE" if node.get("Alive", False) else "DEAD",
            "node_name": "default-name",
            "node_ip": node.get("NodeManagerAddress", ""),
        })

    return result

