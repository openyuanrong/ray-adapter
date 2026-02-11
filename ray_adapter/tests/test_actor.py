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
from unittest.mock import patch, Mock

import yr
import ray_adapter.actor
import ray_adapter.util
from ray_adapter.actor import build_yr_scheduling_options

from ray_adapter.util.placement_group import placement_group


class TestProxyAdaptor(unittest.TestCase):

    def test_ray_function_proxy(self):
        mock_function_proxy = Mock()
        mock_function_proxy.options.return_value = "options"
        mock_function_proxy.create_opts_wrapper.return_value.invoke.return_value = "remote"
        ray_func_proxy = ray_adapter.actor.RemoteFunction(mock_function_proxy)
        ray_func_proxy.options(1)
        res = ray_func_proxy.remote(1)
        self.assertEqual(res, "remote")

    def test_ray_method_proxy(self):
        mock_method_proxy = Mock()
        mock_method_proxy.invoke.return_value = "remote"

        ray_method_proxy = ray_adapter.actor.ActorMethod(mock_method_proxy)
        res = ray_method_proxy.remote(1)
        self.assertEqual(res, "remote")

    @patch.object(yr.InstanceCreator, '_invoke')
    def test_ray_instance_creator(self, mock_invoke):
        class NoBody:
            pass
        mock_invoke.return_value = yr.InstanceProxy.__new__(yr.InstanceProxy)
        ray_instance_creator = ray_adapter.actor.ActorClass._ray_from_modified_class(NoBody, None)
        res = ray_instance_creator.remote(1)
        self.assertIsInstance(res, ray_adapter.actor.ActorHandle)

    def test_build_yr_scheduling_options(self):
        mock_opts = Mock()
        res = build_yr_scheduling_options(actor_opts=mock_opts, name="name", namespace="namespace",
                                          scheduling_strategy=ray_adapter.util.NodeAffinitySchedulingStrategy(
                                              node_id="node_id", soft=True))
        self.assertEqual(len(res.schedule_affinities), 1)
        res = build_yr_scheduling_options(actor_opts=mock_opts, name="name", namespace="namespace",
                                          scheduling_strategy=ray_adapter.util.PlacementGroupSchedulingStrategy(
                                              placement_group=None, placement_group_bundle_index=0))
        self.assertEqual(res.resource_group_options.bundle_index, 0)

    def test_build_yr_scheduling_options_lifetime_detached(self):
        """"""
        mock_opts = Mock()
        mock_opts.custom_extensions = {}
        res = build_yr_scheduling_options(mock_opts, lifetime="detached")
        self.assertEqual(res.custom_extensions["lifecycle"], "detached")

    def test_build_yr_scheduling_options_lifetime_invalid(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(ValueError) as cm:
            build_yr_scheduling_options(mock_opts, lifetime="persistent")
        self.assertIn("only support detached", str(cm.exception))

    def test_build_yr_scheduling_options_lifetime_type_error(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(TypeError):
            build_yr_scheduling_options(mock_opts, lifetime=123)

    def test_build_yr_scheduling_options_num_cpus(self):
        """"""
        mock_opts = Mock()
        mock_opts.cpu = None
        res = build_yr_scheduling_options(mock_opts, num_cpus=2.5)
        self.assertEqual(res.cpu, 2500)

    def test_build_yr_scheduling_options_num_cpus_negative(self):
        """"""
        mock_opts = Mock()
        mock_opts.cpu = None
        res = build_yr_scheduling_options(mock_opts, num_cpus=-1)
        self.assertEqual(res.cpu, 1000)

    def test_build_yr_scheduling_options_num_cpus_type_error(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(TypeError):
            build_yr_scheduling_options(mock_opts, num_cpus="2")

    def test_build_yr_scheduling_options_num_gpus(self):
        """"""
        mock_opts = Mock()
        mock_opts.custom_resources = {}
        res = build_yr_scheduling_options(mock_opts, num_gpus=1.5)
        self.assertEqual(res.custom_resources["GPU/.+/count"], 1.5)

    def test_build_yr_scheduling_options_num_gpus_zero(self):
        """"""
        mock_opts = Mock()
        mock_opts.custom_resources = {}
        res = build_yr_scheduling_options(mock_opts, num_gpus=0.00001)
        self.assertNotIn("GPU/.+/count", res.custom_resources)

    def test_build_yr_scheduling_options_num_gpus_type_error(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(TypeError):
            build_yr_scheduling_options(mock_opts, num_gpus="1")

    def test_build_yr_scheduling_options_max_concurrency(self):
        """"""
        mock_opts = Mock()
        mock_opts.concurrency = None
        res = build_yr_scheduling_options(mock_opts, max_concurrency=4)
        self.assertEqual(res.concurrency, 4)

    def test_build_yr_scheduling_options_max_concurrency_type_error(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(TypeError):
            build_yr_scheduling_options(mock_opts, max_concurrency="4")

    def test_build_yr_scheduling_options_concurrency_groups(self):
        """"""
        mock_opts = Mock()
        mock_opts.concurrency = None
        res = build_yr_scheduling_options(mock_opts, concurrency_groups={"io": 2, "compute": 3})
        self.assertEqual(res.concurrency, 2 + 3 + 1)  # sum + 1

    def test_build_yr_scheduling_options_concurrency_groups_all_none(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(ValueError) as cm:
            build_yr_scheduling_options(mock_opts, concurrency_groups={"a": None, "b": None})
        self.assertIn("All keys in concurrency_groups have None values", str(cm.exception))

    def test_build_yr_scheduling_options_concurrency_groups_type_error(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(TypeError):
            build_yr_scheduling_options(mock_opts, concurrency_groups=["a", "b"])

    def test_build_yr_scheduling_options_default_concurrency(self):
        """"""
        mock_opts = Mock()
        mock_opts.concurrency = None
        res = build_yr_scheduling_options(mock_opts)
        self.assertEqual(res.concurrency, 1)

    def test_build_yr_scheduling_options_custom_resources_memory(self):
        """"""
        mock_opts = Mock()
        mock_opts.memory = None
        mock_opts.custom_resources = {}
        res = build_yr_scheduling_options(mock_opts, resources={"memory": 8192})
        self.assertEqual(res.memory, 8192)
        self.assertNotIn("memory", res.custom_resources)

    def test_build_yr_scheduling_options_custom_resources_npu(self):
        """"""
        mock_opts = Mock()
        mock_opts.custom_resources = {}
        res = build_yr_scheduling_options(mock_opts, resources={"NPU_0": 2})
        self.assertEqual(res.custom_resources["NPU/.+/count"], 2)

    def test_build_yr_scheduling_options_custom_resources_general(self):
        """"""
        mock_opts = Mock()
        mock_opts.custom_resources = {}
        res = build_yr_scheduling_options(mock_opts, resources={"custom_res": 5})
        self.assertEqual(res.custom_resources["custom_res"], 5)

    def test_build_yr_scheduling_options_custom_resources_none_value(self):
        """"""
        mock_opts = Mock()
        with self.assertRaises(ValueError) as cm:
            build_yr_scheduling_options(mock_opts, resources={"bad": None})
        self.assertIn("not a valid resources", str(cm.exception))

    def test_build_yr_scheduling_options_runtime_env(self):
        """"""
        mock_opts = Mock()
        mock_opts.runtime_env = None
        env = {"env_vars": {"KEY": "VALUE"}}
        res = build_yr_scheduling_options(mock_opts, runtime_env=env)
        self.assertEqual(res.runtime_env, env)

if __name__ == "__main__":
    unittest.main()
