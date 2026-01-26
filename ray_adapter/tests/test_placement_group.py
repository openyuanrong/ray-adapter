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
import sys
import os

from unittest.mock import patch, Mock, ANY
from importlib import util
from yr.resource_group import ResourceGroup


def _load_clean_placement_group_module():
    module_name = "_clean_ray_adapter_util_placement_group"
    if module_name in sys.modules:
        return sys.modules[module_name]
    try:
        import ray_adapter.util
        base_dir = os.path.dirname(ray_adapter.util.__file__)
        file_path = os.path.join(base_dir, "placement_group.py")
    except Exception:
        file_path = "ray_adapter/util/placement_group.py"
    spec = util.spec_from_file_location(module_name, file_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load placement_group.py from {file_path}")
    module = util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


pg_mod = _load_clean_placement_group_module()

remove_placement_group = pg_mod.remove_placement_group
PlacementGroup = pg_mod.PlacementGroup
placement_group_table = pg_mod.placement_group_table


class TestPlacementGroup(unittest.TestCase):

    @patch("yr.apis.is_initialized")
    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_create_resource_group_success(self, get_runtime, is_initialized):
        is_initialized.return_value = True
        mock_runtime = Mock()
        mock_runtime.create_resource_group.return_value = "return_req_id"
        get_runtime.return_value = mock_runtime
        res = pg_mod.placement_group([{"CPU": 500}])
        self.assertTrue(res.resource_group.request_id == "return_req_id")

    @patch("yr.apis.is_initialized")
    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_remove_placement_group_success(self, get_runtime, is_initialized):
        placement_group_obj = PlacementGroup(ResourceGroup("name", ""))
        mock_runtime = Mock()
        mock_runtime.remove_resource_group.return_value = "OK"
        get_runtime.return_value = mock_runtime
        try:
            remove_placement_group(placement_group_obj)
        except Exception as e:
            assert False, f"Unexpected exception raised: {e}"

    def test_validate_bundles_invalid_type(self):
        with self.assertRaises(ValueError):
            pg_mod._validate_bundles("not_a_list")

    def test_validate_bundles_empty_list(self):
        with self.assertRaises(ValueError):
            pg_mod._validate_bundles([])

    def test_validate_bundles_invalid_bundle_dict(self):
        with self.assertRaises(ValueError):
            pg_mod._validate_bundles([{"CPU": "invalid"}])

    def test_validate_bundles_all_zero_resources(self):
        with self.assertRaises(ValueError):
            pg_mod._validate_bundles([{"CPU": 0, "Memory": 0}])

    def test_validate_placement_group_invalid_strategy(self):
        with self.assertRaises(ValueError):
            pg_mod.validate_placement_group([{"CPU": 100}], strategy="INVALID")

    def test_validate_placement_group_invalid_lifetime(self):
        with self.assertRaises(ValueError):
            pg_mod.validate_placement_group([{"CPU": 100}], lifetime="permanent")

    def test_resource_key_transformations(self):
        bundles = [{"NPU": 2, "GPU": 1, "memory": 1024, "CPU": 2}]
        pg_mod._validate_bundles(bundles)
        expected = {
            "NPU": 2,
            "GPU": 1,
            "Memory": 1024,
            "CPU": 2000
        }
        self.assertEqual(bundles[0], expected)

    @patch.object(pg_mod, "_get_bundle_cache")
    def test_bundle_specs_lazy_load(self, mock_get_cache):
        mock_get_cache.return_value = [{"CPU": 1000}]
        rg = ResourceGroup(name="test", request_id="")
        pg = PlacementGroup(rg, bundle_cache=None)
        specs = pg.bundle_specs
        self.assertEqual(specs, [{"CPU": 1000}])
        mock_get_cache.assert_called_once_with("test")

    def test_placement_group_properties(self):
        rg = ResourceGroup(name="pg1", request_id="")
        pg = PlacementGroup(rg, bundle_cache=[{"CPU": 1000}])
        self.assertEqual(pg.id, "pg1")
        self.assertEqual(pg.get_id(), "pg1")
        self.assertEqual(pg.bundle_specs, [{"CPU": 1000}])

    def test_placement_group_eq_and_hash(self):
        rg1 = ResourceGroup(name="pg1", request_id="")
        rg2 = ResourceGroup(name="pg1", request_id="")

        def get_id_by_name(instance):
            return instance.name

        import types
        rg1.get_id = types.MethodType(get_id_by_name, rg1)
        rg2.get_id = types.MethodType(get_id_by_name, rg2)
        pg1 = PlacementGroup(rg1)
        pg2 = PlacementGroup(rg2)
        self.assertEqual(pg1, pg2)
        self.assertEqual(hash(pg1), hash(pg2))

        rg3 = ResourceGroup(name="pg2", request_id="")
        pg3 = PlacementGroup(rg3)
        self.assertNotEqual(pg1, pg3)

    @patch.object(pg_mod, "RgObjectRef")
    def test_ready_returns_rg_object_ref(self, mock_rg_ref_class):
        rg = ResourceGroup(name="test", request_id="")
        pg = PlacementGroup(rg)
        result = pg.ready()
        mock_rg_ref_class.assert_called_once_with(rg, ANY)
        self.assertEqual(result, mock_rg_ref_class.return_value)

    @patch.object(ResourceGroup, 'wait')
    def test_wait_success(self, mock_wait):
        rg = ResourceGroup(name="test", request_id="")
        pg = PlacementGroup(rg)
        self.assertTrue(pg.wait(10))
        mock_wait.assert_called_with(10)

    @patch.object(ResourceGroup, 'wait', side_effect=Exception("timeout"))
    def test_wait_failure(self, mock_wait):
        rg = ResourceGroup(name="test", request_id="")
        pg = PlacementGroup(rg)
        self.assertFalse(pg.wait(5))

    @patch("ray_adapter._private.state.state")
    def test_placement_group_table_with_input(self, mock_state):
        mock_state.placement_group_table.return_value = {"state": "CREATED", "bundles": {}}
        rg = ResourceGroup(name="pg1", request_id="")
        pg = PlacementGroup(rg)
        result = placement_group_table(pg)
        mock_state.placement_group_table.assert_called_with("pg1")
        self.assertEqual(result, {"state": "CREATED", "bundles": {}})

    @patch("ray_adapter._private.state.state")
    def test_placement_group_table_without_input(self, mock_state):
        mock_state.placement_group_table.return_value = {}
        result = placement_group_table()
        mock_state.placement_group_table.assert_called_with(None)
        self.assertEqual(result, {})

    def test_get_current_placement_group_no_env(self):
        if "RG_NAME" in os.environ:
            del os.environ["RG_NAME"]
        result = pg_mod.get_current_placement_group()
        self.assertIsNone(result)

    @patch.object(pg_mod, "placement_group_table")
    def test_get_current_placement_group_with_env_valid(self, mock_table):
        os.environ["RG_NAME"] = "current_pg"
        mock_table.return_value = {"state": "CREATED", "bundles": {"0": {"CPU": 1000}}}
        pg = pg_mod.get_current_placement_group()
        self.assertIsNotNone(pg)
        self.assertEqual(pg.get_id(), "current_pg")
        self.assertEqual(pg.resource_group.bundles, {"0": {"CPU": 1000}})

    @patch.object(pg_mod, "placement_group_table")
    def test_get_current_placement_group_removed(self, mock_table):
        os.environ["RG_NAME"] = "removed_pg"
        mock_table.return_value = {"state": "REMOVE"}
        result = pg_mod.get_current_placement_group()
        self.assertIsNone(result)

    @patch("yr.apis.is_initialized")
    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_placement_group_with_custom_name(self, get_runtime, is_initialized):
        is_initialized.return_value = True
        mock_runtime = Mock()
        mock_runtime.create_resource_group.return_value = "req_named"
        get_runtime.return_value = mock_runtime
        pg = pg_mod.placement_group([{"CPU": 100}], name="my_pg")
        self.assertEqual(pg.get_id(), "my_pg")


if __name__ == "__main__":
    unittest.main()