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
import os
import unittest
from collections import defaultdict
from unittest.mock import patch, Mock, MagicMock

import yr
from yr.config_manager import ConfigManager
from yr.common import constants

import ray_adapter
import ray_adapter.worker as worker
import ray_adapter.runtime_context as runtime_context
from ray_adapter.exceptions import GetTimeoutError, RayTaskError
from ray_adapter.worker import (remote, method, wait, available_resources_per_node,
                                cancel, get, get_actor, nodes, available_resources,
                                cluster_resources, get_gpu_ids, _modify_class
                                )
from ray_adapter.actor import RemoteFunction, ActorClass
from ray_adapter.runtime_context import RuntimeContext


class TestIsInitialized(unittest.TestCase):

    @patch('yr.apis.is_initialized')
    def test_is_initialized_true(self, mock_is_initialized):
        mock_is_initialized.return_value = True
        self.assertTrue(worker.is_initialized())

    @patch('yr.apis.is_initialized')
    def test_is_initialized_false(self, mock_is_initialized):
        mock_is_initialized.return_value = False
        self.assertFalse(worker.is_initialized())

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_shutdown(self, get_runtime):
        mock_runtime = Mock()
        mock_runtime.finalize.return_value = None
        mock_runtime.receive_request_loop.return_value = None
        mock_runtime.exit.return_value = None
        get_runtime.return_value = mock_runtime

        # mock ConfigManager().meta_config
        ConfigManager().meta_config = Mock()
        ConfigManager().meta_config.jobID = "test-job-id"

        yr.apis.set_initialized()
        self.assertTrue(worker.is_initialized())
        worker.shutdown()
        self.assertFalse(worker.is_initialized())

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_available_resources_per_node_success(self, mock_get_runtime):
        mock_runtime = Mock()
        mock_runtime.resources.return_value = [
            {"id": "node1", "allocatable": {"cpu": "2", "memory": "2Gi"}},
            {"id": "node2", "allocatable": {"cpu": "4", "memory": "4Gi"}},
        ]
        mock_get_runtime.return_value = mock_runtime

        yr.apis.set_initialized()
        result = worker.available_resources_per_node()

        self.assertEqual(result, {
            "node1": {"cpu": "2", "memory": "2Gi"},
            "node2": {"cpu": "4", "memory": "4Gi"},
        })


class TestKill(unittest.TestCase):

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_kill_with_non_actor(self, _):
        non_actor = "I am not an actor"
        yr.apis.set_initialized()
        with self.assertRaises(ValueError):
            worker.kill(non_actor)

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_kill_with_actor_no_terminate(self, _):
        actor = Mock()
        del actor.terminate
        yr.apis.set_initialized()
        with self.assertRaises(ValueError):
            worker.kill(actor)

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_kill_with_actor_terminate_not_callable(self, _):
        actor = Mock()
        actor.terminate = "I am not callable"
        yr.apis.set_initialized()
        with self.assertRaises(ValueError):
            worker.kill(actor)

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_kill_with_actor_terminate_success(self, _):
        actor = Mock()
        actor.terminate.return_value = True
        with patch("ray_adapter.worker.ActorHandle", new=type(actor)):
            yr.apis.set_initialized()
            self.assertTrue(worker.kill(actor))
            actor.terminate.assert_called_once_with(is_sync=True)

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_kill_with_actor_terminate_exception(self, _):
        actor = Mock()
        actor.terminate.side_effect = Exception("terminate failed")
        yr.apis.set_initialized()
        with patch("ray_adapter.worker.isinstance", return_value=True):
            with self.assertLogs(level='WARNING') as log:
                self.assertIsNone(worker.kill(actor))
            self.assertIn("Failed to kill actor", log.output[0])
            actor.terminate.assert_called_once_with(is_sync=True)


def sample_func(x):
    return x + 1


def my_func():
    return "hello"


class TestMethodAdaptor(unittest.TestCase):

    @patch("logging.warning")
    def test_method_decorate_function(self, mock_warning):
        result = worker.method(sample_func)
        self.assertEqual(result, sample_func)
        mock_warning.assert_called_once()
        args, kwargs = mock_warning.call_args
        self.assertIn("method only supports member methods", args[0])

    @patch("yr.apis.method")
    def test_method_call_underlying(self, mock_apis_method):
        mock_apis_method.return_value = "ok"
        result = worker.method("class_instance", 1, 2, key="value")
        mock_apis_method.assert_called_once_with("class_instance", 1, 2, key="value")
        self.assertEqual(result, "ok")

    @patch("logging.warning")
    def test_method_decorator_warning(self, mock_warning):
        result = worker.method(my_func)
        self.assertIs(result, my_func)

        mock_warning.assert_called_once()
        called_args = mock_warning.call_args[0]
        self.assertIn(
            "method only supports member methods in this runtime", str(called_args)
        )


class TestGetFunction(unittest.TestCase):

    @patch('ray_adapter.worker.yr.apis.get')
    def test_get_single_object_ref(self, mock_yr_get):
        obj_ref = MagicMock()
        mock_yr_get.return_value = 42

        result = get(obj_ref)

        mock_yr_get.assert_called_once_with(obj_ref, constants.NO_LIMIT)
        self.assertEqual(result, 42)

    @patch('ray_adapter.worker.yr.apis.get')
    def test_get_multiple_object_refs(self, mock_yr_get):
        obj_refs = [MagicMock(), MagicMock()]
        mock_yr_get.return_value = [10, 20]

        result = get(obj_refs)

        mock_yr_get.assert_called_once_with(obj_refs, constants.NO_LIMIT)
        self.assertEqual(result, [10, 20])

    @patch('ray_adapter.worker.yr.apis.get')
    def test_get_with_positive_timeout(self, mock_yr_get):
        obj_ref = MagicMock()
        mock_yr_get.return_value = "result"

        result = get(obj_ref, timeout=5.0)

        mock_yr_get.assert_called_once_with(obj_ref, 5.0)
        self.assertEqual(result, "result")

    def test_get_timeout_zero_raises_get_timeout_error(self):
        obj_ref = MagicMock()

        with self.assertRaises(GetTimeoutError) as cm:
            get(obj_ref, timeout=0)

        self.assertIn("cannot return object immediately", str(cm.exception))

    @patch('ray_adapter.worker.yr.apis.get')
    @patch('ray_adapter.exceptions.get_node_ip_address', return_value="127.0.0.1")
    def test_get_other_exception_wrapped_in_ray_task_error(self, mock_get_ip, mock_yr_get):
        obj_ref = MagicMock()
        original_error = RuntimeError("network down")
        mock_yr_get.side_effect = original_error

        with self.assertRaises(RayTaskError) as cm:
            get(obj_ref)

        self.assertEqual(cm.exception.function_name, "get")
        self.assertIs(cm.exception.cause, original_error)
        self.assertIn("network down", cm.exception.traceback_str)


class TestInit(unittest.TestCase):

    @patch('yr.init')
    def test_init_with_default_values(self, mock_yr_init):
        worker.init()
        conf_arg = mock_yr_init.call_args[0][0]
        self.assertEqual(conf_arg.log_level, "WARNING")

    @patch('yr.init')
    def test_init_with_custom_logging_level(self, mock_yr_init):
        worker.init(logging_level=40)
        conf_arg = mock_yr_init.call_args[0][0]
        self.assertEqual(conf_arg.log_level, "ERROR")

    @patch('yr.init')
    def test_init_with_custom_num_cpus(self, mock_yr_init):
        worker.init(num_cpus=4)
        conf_arg = mock_yr_init.call_args[0][0]
        self.assertEqual(conf_arg.num_cpus, 4)

    @patch('yr.init')
    def test_init_with_runtime_env(self, mock_yr_init):
        env = {'key': 'value'}
        worker.init(runtime_env=env)
        conf_arg = mock_yr_init.call_args[0][0]
        self.assertEqual(conf_arg.runtime_env, env)


class TestHelpers(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._runtime_ctx_instance = runtime_context.get_runtime_context()
        runtime_context.get_accelerator_ids = cls._runtime_ctx_instance.get_accelerator_ids
        runtime_context.get_node_id = cls._runtime_ctx_instance.get_node_id
        cls._namespace_patch = patch("yr.apis.get_namespace", return_value="")
        cls._namespace_patch.start()
        runtime_context.namespace = runtime_context.get_runtime_context().namespace

    @classmethod
    def tearDownClass(cls):
        cls._namespace_patch.stop()

    @patch.dict(os.environ, {
        "YR_NOSET_ASCEND_RT_VISIBLE_DEVICES": "1",
        "NPU-DEVICE-IDS": "0,1"
    }, clear=True)
    def test_get_accelerator_ids_npu_device_ids(self):
        acc_ids = runtime_context.get_accelerator_ids()
        self.assertIn("NPU", acc_ids)
        self.assertEqual(acc_ids["NPU"], ["0", "1"])

    @patch.dict(os.environ, {
        "ASCEND_RT_VISIBLE_DEVICES": "2,3"
    }, clear=True)
    def test_get_accelerator_ids_ascend_visible(self):
        acc_ids = runtime_context.get_accelerator_ids()
        self.assertIn("NPU", acc_ids)
        self.assertEqual(acc_ids["NPU"], ["2", "3"])

    @patch.dict(os.environ, {
        "DEVICE_INFO": "gpu0,gpu1"
    }, clear=True)
    def test_get_accelerator_ids_device_info(self):
        acc_ids = runtime_context.get_accelerator_ids()
        self.assertIn("device", acc_ids)
        self.assertEqual(acc_ids["device"], ["gpu0", "gpu1"])

    @patch("yr.runtime_holder.global_runtime.get_runtime")
    def test_get_node_id(self, mock_get_runtime):
        mock_runtime = Mock()
        mock_runtime.get_node_id.return_value = "node_123"
        mock_get_runtime.return_value = mock_runtime

        yr.apis.set_initialized()
        result = runtime_context.get_node_id()
        self.assertEqual(result, "node_123")

    def test_namespace(self):
        result = runtime_context.namespace
        self.assertEqual(result, "", f"Expected empty string, got {repr(result)}")
        self.assertIsInstance(result, str)


class TestRemote(unittest.TestCase):
    def test_remote_with_invalid_args(self):
        with self.assertRaises(TypeError):
            @remote(max_retries="invalid")
            def test_function():
                pass
        with self.assertRaises(ValueError):
            @remote(max_retries=-1)
            def test_function():
                pass
        with self.assertRaises(TypeError):
            @remote(num_cpus="invalid")
            def test_function():
                pass

    def test_remote_with_callable(self):
        @remote
        def test_function():
            pass

        self.assertIsInstance(test_function, RemoteFunction)
        inner_proxy = test_function._RemoteFunction__function_proxy
        self.assertTrue(hasattr(inner_proxy, "invoke_options"))
        self.assertEqual(inner_proxy.invoke_options.concurrency, 1)
        self.assertEqual(inner_proxy.invoke_options.custom_resources, {})

    def test_remote_with_custom_resources_and_concurrency(self):
        @remote(resources={"NPU": 0.7}, concurrency_groups={"acquire": 1, "release": 10})
        def test_function():
            pass

        self.assertIsInstance(test_function, RemoteFunction)
        inner_proxy = test_function._RemoteFunction__function_proxy
        self.assertTrue(hasattr(inner_proxy, "invoke_options"))
        self.assertEqual(inner_proxy.invoke_options.custom_resources, {"NPU/.+/count": 0.7})
        self.assertEqual(inner_proxy.invoke_options.concurrency, 12)

    def test_remote_num_npus_boundaries(self):
        @remote(num_gpus=0.001)
        def fn_small():
            pass

        inner_proxy = fn_small._RemoteFunction__function_proxy
        self.assertEqual(inner_proxy.invoke_options.custom_resources.get("GPU/.+/count"), 0.001)

    def test_remote_num_cpus(self):
        with self.assertRaises(TypeError):
            @remote(num_cpus=-1)
            def func():
                pass
        with self.assertRaises(TypeError):
            @remote(num_cpus='x')
            def fn():
                pass

        @remote(num_cpus=2.2)
        def cpu_float():
            pass

        inner_proxy = cpu_float._RemoteFunction__function_proxy
        self.assertEqual(inner_proxy.invoke_options.cpu, 2200.0)


class TestWait(unittest.TestCase):
    @patch('yr.apis.wait')
    def test_wait_input_list(self, mock_wait):
        ref1 = ray_adapter.ObjectRef("1")
        ref2 = ray_adapter.ObjectRef("2")
        ref3 = ray_adapter.ObjectRef("3")
        input_refs = [ref1, ref2, ref3]
        mock_wait.return_value = ([ref3, ref2, ref1], [])
        ready_refs, unready_refs = wait(input_refs)
        self.assertEqual(ready_refs, [ref3, ref2, ref1])
        self.assertEqual(unready_refs, [])

    @patch('yr.apis.wait')
    def test_wait_with_wait_num_and_timeout(self, mock_wait):
        ref1 = ray_adapter.ObjectRef("a")
        ref2 = ray_adapter.ObjectRef("b")
        refs = [ref1, ref2]
        mock_wait.return_value = ([ref1], [ref2])
        ready, unready = wait(refs, wait_num=1, timeout=5.7)
        mock_wait.assert_called_once_with(refs, 1, 5)
        self.assertEqual(ready, [ref1])
        self.assertEqual(unready, [ref2])

    @patch('yr.apis.wait')
    def test_wait_empty_list(self, mock_wait):
        mock_wait.return_value = ([], [])
        ready, unready = wait([])
        mock_wait.assert_called_once_with([], None, None)
        self.assertEqual(ready, [])
        self.assertEqual(unready, [])

    @patch('yr.apis.wait')
    def test_wait_wait_num_none_is_passed_as_none(self, mock_wait):
        ref = ray_adapter.ObjectRef("y")
        mock_wait.return_value = ([ref], [])
        wait([ref], wait_num=None)
        mock_wait.assert_called_once_with([ref], None, None)


class TestAvailableResource(unittest.TestCase):
    @patch('yr.apis.resources')
    def test_available_resources_per_node_memory(self, mock_resources):
        mock_resources.return_value = [
            {
                "id": "node-1",
                "allocatable": {
                    "Memory": 12,
                    "NPU-0": 2,
                    "GPU-1": 3
                }
            }

        ]
        result = available_resources_per_node()
        node = result["node-1"]
        self.assertEqual(node["memory"], 12)

    @patch('yr.apis.resources')
    def test_available_resources_per_node_npu(self, mock_resources):
        mock_resources.return_value = [
            {
                "id": "node-1",
                "allocatable": {
                    "Memory": 12,
                    "NPU-0": 2,
                    "GPU-1": 3
                }
            }

        ]
        result = available_resources_per_node()
        node = result["node-1"]
        self.assertEqual(node["NPU"], 2)

    @patch('yr.apis.resources')
    def test_available_resources_per_node_gpu(self, mock_resources):
        mock_resources.return_value = [
            {
                "id": "node-1",
                "allocatable": {
                    "Memory": 12,
                    "NPU-0": 2,
                    "GPU-1": 3
                }
            }

        ]
        result = available_resources_per_node()
        node = result["node-1"]
        self.assertEqual(node["GPU"], 3)


class TestCancel(unittest.TestCase):
    @patch('yr.cancel')
    def test_cancel_single(self, mock_yr_cancel):
        ref = ray_adapter.ObjectRef("123")
        cancel(ref, force=True, recursive=False)
        mock_yr_cancel.assert_called_once_with(
            obj_refs=ref,
            allow_force=True,
            allow_recursive=False
        )


class TestMethod(unittest.TestCase):
    class TestMethod(unittest.TestCase):
        def test_method_decorator(self):
            def dummy_method(self):
                return 42

            with patch("ray_adapter.worker.yr.apis.method") as mock_yr_method:
                mock_yr_method.return_value = "decorated_method"

                decorated1 = method(dummy_method)
                mock_yr_method.assert_called_with(dummy_method)
                assert decorated1 == "decorated_method"

                mock_yr_method.reset_mock()
                decorator = method()
                decorated2 = decorator(dummy_method)
                mock_yr_method.assert_called_with(dummy_method)
                assert decorated2 == "decorated_method"

                mock_yr_method.reset_mock()

                decorator3 = method(num_returns=2)
                decorated3 = decorator3(dummy_method)
                mock_yr_method.assert_called_with(dummy_method, return_nums=2)
                assert decorated3 == "decorated_method"

        def test_method_on_plain_function(self):
            def f():
                pass

            with patch("logging.warning") as mock_warn:
                result = method(f)
                assert result is f
                mock_warn.assert_called()

        def test_method_on_member_method_no_parentheses(self):
            def f(self):
                pass

            with patch("ray_adapter.worker.yr.apis.method") as mock_api:
                mock_api.return_value = "ok"
                result = method(f)
                mock_api.assert_called_once_with(f)
                assert result == "ok"


class TestGetActor(unittest.TestCase):
    def test_get_actor_empty_name_raises_value_error(self):
        with self.assertRaises(ValueError) as cm:
            get_actor("")
        self.assertIn("valid name", str(cm.exception))

    def test_get_actor_invalid_namespace_type_raises_type_error(self):
        with self.assertRaises(TypeError) as cm:
            get_actor("test_actor", namespace=123)
        self.assertIn("namespace must be None or a string", str(cm.exception))

    def test_get_actor_empty_namespace_string_raises_value_error(self):
        with self.assertRaises(ValueError) as cm:
            get_actor("test_actor", namespace="")
        self.assertIn('"" is not a valid namespace', str(cm.exception))

    @patch("ray_adapter.worker.get_instance")
    def test_get_actor_success_with_namespace(self, mock_get_instance):
        mock_instance_proxy = Mock()
        mock_get_instance.return_value = mock_instance_proxy

        handle = get_actor("test_actor", namespace="default")

        mock_get_instance.assert_called_once_with("test_actor", "default")
        self.assertIsInstance(handle, ray_adapter.actor.ActorHandle)

    @patch("ray_adapter.worker.get_instance")
    def test_get_actor_success_without_namespace(self, mock_get_instance):
        mock_instance_proxy = Mock()
        mock_get_instance.return_value = mock_instance_proxy

        handle = get_actor("test_actor")

        mock_get_instance.assert_called_once_with("test_actor", "")
        self.assertIsInstance(handle, ray_adapter.actor.ActorHandle)

    @patch("ray_adapter.worker.get_instance")
    def test_get_actor_instance_not_found_propagates_exception(self, mock_get_instance):
        mock_get_instance.side_effect = ValueError("Actor 'xxx' not found")

        with self.assertRaises(ValueError) as cm:
            get_actor("xxx")
        self.assertIn("not found", str(cm.exception))


class TestNodes(unittest.TestCase):

    @patch("ray_adapter.worker.resources")
    def test_nodes_normal_case(self, mock_resources):
        mock_resources.return_value = [
            {
                "labels": {
                    "HOST_IP": ["192.168.1.10"],
                    "NODE_ID": ["node-abc123"]
                },
                "status": 0,
                "capacity": {
                    "NPU_0": 4,
                    "NPU_1": 4,
                    "GPU_0": 2,
                    "MemoryMB": 16384,
                    "CPU": 8
                }
            },
            {
                "labels": {
                    "HOST_IP": [],
                    "NODE_ID": ["node-def456"]
                },
                "status": 1,  # not alive
                "capacity": {
                    "GPU_0": 1,
                    "MemoryMB": 8192,
                    "CustomRes": 10
                }
            }
        ]

        result = nodes()
        self.assertEqual(len(result), 2)
        node0 = result[0]
        self.assertEqual(node0["NodeID"], "node-abc123")
        self.assertTrue(node0["Alive"])
        self.assertEqual(node0["NodeManagerAddress"], "192.168.1.10")
        self.assertEqual(node0["Resources"]["NPU"], 8)
        self.assertEqual(node0["Resources"]["GPU"], 2)
        self.assertEqual(node0["Resources"]["memory"], 16384)
        self.assertEqual(node0["Resources"]["CPU"], 8)

        node1 = result[1]
        self.assertEqual(node1["NodeID"], "node-def456")
        self.assertFalse(node1["Alive"])  # status=1 ≠ 0
        self.assertEqual(node1["NodeManagerAddress"], "")  # HOST_IP empty
        self.assertEqual(node1["Resources"]["GPU"], 1)
        self.assertEqual(node1["Resources"]["memory"], 8192)
        self.assertEqual(node1["Resources"]["CustomRes"], 10)

        self.assertIn("HOST_IP", node1["labels"])
        self.assertEqual(node1["labels"]["NODE_ID"], ["node-def456"])

    @patch("ray_adapter.worker.resources")
    def test_nodes_empty_resources(self, mock_resources):
        mock_resources.return_value = []
        result = nodes()
        self.assertEqual(result, [])

    @patch("ray_adapter.worker.resources")
    def test_nodes_missing_labels_or_capacity(self, mock_resources):
        mock_resources.return_value = [
            {
                "labels": {},  # empty labels
                "status": 0,
                "capacity": {}  # no resources
            }
        ]
        result = nodes()
        node = result[0]
        self.assertEqual(node["NodeID"], "")
        self.assertEqual(node["NodeManagerAddress"], "")
        self.assertEqual(node["Resources"], {})
        self.assertEqual(node["labels"], {})
        self.assertTrue(node["Alive"])  # status == 0

    @patch("ray_adapter.worker.resources")
    def test_nodes_no_status_field(self, mock_resources):
        mock_resources.return_value = [
            {
                "labels": {},
                "capacity": {}
                # status missing
            }
        ]
        result = nodes()
        node = result[0]
        self.assertFalse(node["Alive"])


class TestAvailableResources(unittest.TestCase):

    @patch("ray_adapter.worker.resources")
    def test_available_resources_normal_case(self, mock_resources):
        mock_resources.return_value = [
            {
                "allocatable": {
                    "NPU_0": 2.0,
                    "NPU_1": 2.0,
                    "GPU_0": 1.0,
                    "MemoryMB": 8192.0,
                    "CPU": 4.0,
                    "CustomRes": 5.0
                }
            },
            {
                "allocatable": {
                    "NPU_2": 4.0,
                    "GPU_0": 0.0,
                    "MemoryMB": 4096.0,
                    "CPU": 2.0,
                    "ZeroRes": 1e-7
                }
            }
        ]

        result = available_resources()
        self.assertIsInstance(result, dict)
        self.assertNotIsInstance(result, defaultdict)

        self.assertAlmostEqual(result["NPU"], 8.0)
        self.assertAlmostEqual(result["GPU"], 1.0)
        self.assertAlmostEqual(result["memory"], 12288.0)
        self.assertAlmostEqual(result["CPU"], 6.0)
        self.assertAlmostEqual(result["CustomRes"], 5.0)
        self.assertNotIn("ZeroRes", result)
        self.assertNotIn("GPU_0", result)

    @patch("ray_adapter.worker.resources")
    def test_available_resources_empty_input(self, mock_resources):
        mock_resources.return_value = []
        result = available_resources()
        self.assertEqual(result, {})

    @patch("ray_adapter.worker.resources")
    def test_available_resources_all_zero_values(self, mock_resources):
        mock_resources.return_value = [
            {
                "allocatable": {
                    "NPU_0": 0.0,
                    "GPU_0": 1e-8,
                    "MemoryMB": -1e-9,
                    "CPU": 0.0
                }
            }
        ]
        result = available_resources()
        self.assertEqual(result, {})

    @patch("ray_adapter.worker.resources")
    def test_available_resources_negative_values_allowed(self, mock_resources):
        mock_resources.return_value = [
            {
                "allocatable": {
                    "CPU": -2.0,
                    "BadRes": -1e-5
                }
            }
        ]
        result = available_resources()
        self.assertAlmostEqual(result["CPU"], -2.0)
        self.assertAlmostEqual(result["BadRes"], -1e-5)

    @patch("ray_adapter.worker.resources")
    def test_available_resources_no_allocatable_key(self, mock_resources):
        pass


class TestClusterResources(unittest.TestCase):

    @patch("ray_adapter.worker.resources")
    def test_cluster_resources_normal_case(self, mock_resources):
        mock_resources.return_value = [
            {
                "capacity": {
                    "NPU_0": 4.0,
                    "NPU_1": 4.0,
                    "GPU_0": 2.0,
                    "MemoryMB": 16384.0,
                    "CPU": 8.0,
                    "CustomRes": 10.0
                }
            },
            {
                "capacity": {
                    "NPU_2": 8.0,
                    "GPU_0": 1.0,
                    "MemoryMB": 8192.0,
                    "CPU": 4.0,
                    "Disk": 500.0
                }
            }
        ]

        result = cluster_resources()

        self.assertIsInstance(result, dict)
        self.assertNotIsInstance(result, defaultdict)

        self.assertAlmostEqual(result["NPU"], 16.0)
        self.assertAlmostEqual(result["GPU"], 3.0)
        self.assertAlmostEqual(result["memory"], 24576.0)
        self.assertAlmostEqual(result["CPU"], 12.0)
        self.assertAlmostEqual(result["CustomRes"], 10.0)
        self.assertAlmostEqual(result["Disk"], 500.0)

    @patch("ray_adapter.worker.resources")
    def test_cluster_resources_empty_input(self, mock_resources):
        mock_resources.return_value = []
        result = cluster_resources()
        self.assertEqual(result, {})

    @patch("ray_adapter.worker.resources")
    def test_cluster_resources_zero_and_negative_values_included(self, mock_resources):
        # 注意：cluster_resources 不过滤小值或负值！
        mock_resources.return_value = [
            {
                "capacity": {
                    "NPU_0": 0.0,
                    "GPU_0": -1.0,
                    "MemoryMB": 1e-7,  # 极小但保留
                    "CPU": -2.5
                }
            }
        ]
        result = cluster_resources()
        self.assertAlmostEqual(result["NPU"], 0.0)
        self.assertAlmostEqual(result["GPU"], -1.0)
        self.assertAlmostEqual(result["memory"], 1e-7)
        self.assertAlmostEqual(result["CPU"], -2.5)

    @patch("ray_adapter.worker.resources")
    def test_cluster_resources_case_insensitive_resource_keys(self, mock_resources):
        mock_resources.return_value = [
            {
                "capacity": {
                    "npu_0": 2.0,
                    "NPU_1": 2.0,
                    "MemoryGB": 16.0,
                    "gpu": 1.0
                }
            }
        ]
        result = cluster_resources()
        self.assertAlmostEqual(result["NPU"], 2.0)
        self.assertAlmostEqual(result["memory"], 16.0)
        self.assertAlmostEqual(result["npu_0"], 2.0)
        self.assertAlmostEqual(result["gpu"], 1.0)
        self.assertNotIn("GPU", result)

    @patch("ray_adapter.worker.resources")
    def test_cluster_resources_duplicate_custom_keys_across_nodes(self, mock_resources):
        mock_resources.return_value = [
            {"capacity": {"WorkerSlots": 2.0}},
            {"capacity": {"WorkerSlots": 3.0}}
        ]
        result = cluster_resources()
        self.assertAlmostEqual(result["WorkerSlots"], 5.0)


class TestGetGpuIds(unittest.TestCase):

    def test_get_gpu_ids_env_not_set(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(get_gpu_ids(), [])

    def test_get_gpu_ids_env_empty_string(self):
        with patch.dict(os.environ, {"GPU-DEVICE-IDS": ""}):
            self.assertEqual(get_gpu_ids(), [])

    def test_get_gpu_ids_normal_case(self):
        with patch.dict(os.environ, {"GPU-DEVICE-IDS": "0,1,2"}):
            self.assertEqual(get_gpu_ids(), [0, 1, 2])

    def test_get_gpu_ids_single_id(self):
        with patch.dict(os.environ, {"GPU-DEVICE-IDS": "3"}):
            self.assertEqual(get_gpu_ids(), [3])

    def test_get_gpu_ids_with_spaces(self):
        with patch.dict(os.environ, {"GPU-DEVICE-IDS": "0, 1, 2"}):
            self.assertEqual(get_gpu_ids(), [0, 1, 2])

    def test_get_gpu_ids_all_whitespace(self):
        with patch.dict(os.environ, {"GPU-DEVICE-IDS": "   ,  ,"}):
            with self.assertRaises(ValueError):
                get_gpu_ids()

    def test_get_gpu_ids_invalid_integer(self):
        with patch.dict(os.environ, {"GPU-DEVICE-IDS": "0,abc,2"}):
            with self.assertRaises(ValueError):
                get_gpu_ids()


class TestRuntimeContext(unittest.TestCase):

    def test_get_actor_id_nil(self):
        with patch.dict(os.environ, {"INSTANCE_ID": "nil"}):
            ctx = RuntimeContext()
            self.assertIsNone(ctx.get_actor_id())

    def test_get_actor_id_valid(self):
        with patch.dict(os.environ, {"INSTANCE_ID": "my-actor-123"}):
            ctx = RuntimeContext()
            self.assertEqual(ctx.get_actor_id(), "my-actor-123")


class TestModifyClass(unittest.TestCase):

    def test_already_modified_class(self):
        class Original:
            __ray_actor_class__ = "modified"

        result = _modify_class(Original)
        self.assertIs(result, Original)

    def test_adds_ray_methods(self):
        class PlainClass:
            pass

        Modified = _modify_class(PlainClass)
        instance = Modified()

        self.assertTrue(hasattr(instance, '__ray_ready__'))
        self.assertTrue(hasattr(instance, '__ray_call__'))
        self.assertTrue(hasattr(instance, '__ray_terminate__'))

        self.assertTrue(instance.__ray_ready__())

        def dummy_method(self, x):
            return x * 2

        result = instance.__ray_call__(dummy_method, 5)
        self.assertEqual(result, 10)

    def test_preserves_original_module_and_name(self):
        class OriginalClass:
            pass

        OriginalClass.__module__ = "my.custom.module"
        Modified = _modify_class(OriginalClass)

        self.assertEqual(Modified.__name__, "OriginalClass")
        self.assertEqual(Modified.__module__, "my.custom.module")

    def test_adds_default_init_if_missing(self):
        class NoInitClass:
            pass

        self.assertIs(NoInitClass.__init__, object.__init__)

        Modified = _modify_class(NoInitClass)
        instance = Modified()
        self.assertIsNot(Modified.__init__, object.__init__)

    def test_preserves_existing_init(self):
        class WithInit:
            def __init__(self, value):
                self.value = value

        Modified = _modify_class(WithInit)
        instance = Modified(42)
        self.assertEqual(instance.value, 42)


if __name__ == "__main__":
    unittest.main()
