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

import unittest
from unittest.mock import patch, Mock, MagicMock

import yr
from yr.common.types import CommonStatus, Resource, Resources, BundleInfo, Option, RgInfo, ResourceGroupUnit

import ray_adapter._private.state
from ray_adapter._private.state import build_pg_dict, parse_pg_info_to_dict, GlobalState, available_resources_per_node


class TestBuildPgDict(unittest.TestCase):
    def setUp(self):
        self.gpu_resource = Mock(spec=Resource)
        self.gpu_resource.type = Resource.Type.SCALER
        self.gpu_resource.name = "GPU"
        self.gpu_resource.scalar = Mock(value=2)

        self.mem_resource = Mock(spec=Resource)
        self.mem_resource.type = Resource.Type.SCALER
        self.mem_resource.name = "Memory"
        self.mem_resource.scalar = Mock(value=10)

        self.other_resource = Mock(spec=Resource)
        self.other_resource.type = Resource.Type.SCALER
        self.other_resource.name = "OtherResource"
        self.other_resource.scalar = Mock(value=1)

        self.resources_obj = Mock()
        self.resources_obj.resources = {
            'GPU': self.gpu_resource,
            'Memory': self.mem_resource,
            'OtherResource': self.other_resource
        }

        self.bundle = Mock(spec=BundleInfo)
        self.bundle.resources = self.resources_obj
        self.bundle.functionProxyId = 'node_123'

        self.rg_info = Mock(spec=RgInfo)
        self.rg_info.name = 'test_pg'
        self.rg_info.bundles = [self.bundle]
        self.rg_info.status = Mock(spec=CommonStatus, code=1)
        self.rg_info.opt = Mock(spec=Option, groupPolicy=3)

    def test_build_pg_dict_with_gpu(self):
        result = build_pg_dict(self.rg_info)
        bundle0 = result['bundles'][0]
        self.assertEqual(bundle0['GPU'], 2)
        self.assertEqual(bundle0['memory'], 10)
        self.assertEqual(bundle0['OtherResource'], 1)


class TestParsePgInfoToDict(unittest.TestCase):

    @patch('ray_adapter._private.state.build_pg_dict')  # ← 只 mock 依赖，不 mock 自己！
    def test_empty_resource_groups(self, mock_build_pg_dict):
        rg_unit = MagicMock()
        rg_unit.resourceGroups = {}

        result = parse_pg_info_to_dict(rg_unit)

        self.assertEqual(result, {})
        mock_build_pg_dict.assert_not_called()

    @patch('ray_adapter._private.state.build_pg_dict')
    def test_single_resource_group(self, mock_build_pg_dict):
        mock_pg = Mock()  # 更真实的模拟对象
        rg_unit = MagicMock()
        rg_unit.resourceGroups = {"group1": mock_pg}
        mock_build_pg_dict.return_value = {"name": "group1_detail"}

        result = parse_pg_info_to_dict(rg_unit)

        self.assertEqual(result, {"name": "group1_detail"})
        mock_build_pg_dict.assert_called_once_with(mock_pg)

    @patch('ray_adapter._private.state.build_pg_dict')
    def test_multiple_resource_groups(self, mock_build_pg_dict):
        pg1, pg2 = Mock(), Mock()
        rg_unit = MagicMock()
        rg_unit.resourceGroups = {"A": pg1, "B": pg2}

        def mock_build_pg_dict_return(pg_obj):
            return {"detail": str(pg_obj)}

        pg1, pg2 = Mock(), Mock()
        rg_unit = MagicMock()
        rg_unit.resourceGroups = {"A": pg1, "B": pg2}

        mock_build_pg_dict.side_effect = mock_build_pg_dict_return
        result = parse_pg_info_to_dict(rg_unit)

        expected = {
            "A": {"detail": str(pg1)},
            "B": {"detail": str(pg2)}
        }
        self.assertEqual(result, expected)
        self.assertEqual(mock_build_pg_dict.call_count, 2)
        mock_build_pg_dict.assert_any_call(pg1)
        mock_build_pg_dict.assert_any_call(pg2)

    @patch('ray_adapter._private.state.build_pg_dict')
    def test_single_group_with_non_string_key(self, mock_build_pg_dict):
        rg_unit = MagicMock()
        rg_unit.resourceGroups = {42: Mock()}
        mock_build_pg_dict.return_value = {"value": "ok"}

        result = parse_pg_info_to_dict(rg_unit)

        self.assertEqual(result, {"value": "ok"})
        mock_build_pg_dict.assert_called_once()


class TestGlobalState(unittest.TestCase):

    @patch('ray_adapter._private.state.parse_pg_info_to_dict')
    @patch('ray_adapter._private.state.resource_group_table')
    def test_placement_group_table(self, mock_resource_group_table, mock_parse_pg_info_to_dict):
        pg_id = "pg_123"
        fake_rg_unit = MagicMock()
        fake_result = {"pg": "info"}

        mock_resource_group_table.return_value = fake_rg_unit
        mock_parse_pg_info_to_dict.return_value = fake_result

        result = GlobalState().placement_group_table(pg_id)
        mock_resource_group_table.assert_called_once_with(pg_id)
        mock_parse_pg_info_to_dict.assert_called_once_with(fake_rg_unit)
        self.assertEqual(result, fake_result)


class TestAvailableResourcesPerNode(unittest.TestCase):

    @patch("ray_adapter._private.state.resources")
    def test_available_resources_per_node(self, mock_resources):
        mock_resources.return_value = [
            {
                "id": "node-1",
                "allocatable": {
                    "CPU": 4,
                    "Memory": 8192,
                    "GPU": 2,
                    "NPU-0": 1,
                    "CustomRes": 10
                }
            },
            {
                "id": "node-2",
                "allocatable": {
                    "CPU": 8,
                    "Memory": 16384,
                    "GPU-1": 4,
                    "NPU": 2
                }
            }
        ]

        result = available_resources_per_node()

        expected = {
            "node-1": {
                "CPU": 4,
                "memory": 8192,  # Memory → memory
                "GPU": 2,  # GPU → GPU (kept)
                "NPU": 1,  # NPU-0 → NPU
                "CustomRes": 10
            },
            "node-2": {
                "CPU": 8,
                "memory": 16384,  # Memory → memory
                "GPU": 4,  # GPU-1 → GPU
                "NPU": 2  # NPU → NPU
            }
        }

        self.assertEqual(result, expected)

    @patch("ray_adapter._private.state.resources")
    def test_available_resources_per_node_empty_input(self, mock_resources):
        mock_resources.return_value = []

        result = available_resources_per_node()

        self.assertEqual(result, {})

    @patch("ray_adapter._private.state.resources")
    def test_available_resources_per_node_missing_allocatable(self, mock_resources):
        mock_resources.return_value = [
            {"id": "node-3"}  # no 'allocatable'
        ]

        result = available_resources_per_node()

        self.assertEqual(result, {"node-3": {}})

    @patch("ray_adapter._private.state.resources")
    def test_available_resources_per_node_missing_id(self, mock_resources):
        mock_resources.return_value = [
            {"allocatable": {"CPU": 2}}
        ]

        result = available_resources_per_node()

        self.assertEqual(result, {None: {"CPU": 2}})


if __name__ == "__main__":
    unittest.main()
