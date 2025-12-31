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
from unittest.mock import patch
from ray_adapter.exceptions import RayTaskError, YRErrorWithCause


class UnpicklableException(Exception):
    def __reduce__(self):
        raise TypeError("Not picklable")


@patch('ray_adapter.exceptions.get_node_ip_address', return_value="127.0.0.1")
class TestRayTaskError(unittest.TestCase):

    def test_ray_task_wrap_success(self, mock_get_ip):
        @RayTaskError.ray_task_wrap
        def good_func():
            return "ok"
        self.assertEqual(good_func(), "ok")

    def test_ray_task_wrap_raises_raytaskerror_on_exception(self, mock_get_ip):
        @RayTaskError.ray_task_wrap
        def bad_func():
            raise ValueError("test error")

        with self.assertRaises(RayTaskError) as cm:
            bad_func()

        exc = cm.exception
        self.assertIn("test error", str(exc))
        self.assertEqual(exc.function_name, "bad_func")
        self.assertIsInstance(exc.cause, ValueError)
        self.assertEqual(exc.ip, "127.0.0.1")

    def test_yr_invoke_error_conversion(self, mock_get_ip):
        class FakeYRInvokeError(Exception):
            pass
        FakeYRInvokeError.__qualname__ = "YRInvokeError(RuntimeError)"

        cause = FakeYRInvokeError("simulated error")
        error = RayTaskError("test_func", "fake tb", cause)

        self.assertIsInstance(error.cause, RuntimeError)
        self.assertEqual(str(error.cause), "simulated error")
        self.assertEqual(error.ip, "127.0.0.1")

    def test_unpicklable_cause_fallback_to_yrerrorwithcause(self, mock_get_ip):
        cause = UnpicklableException()
        error = RayTaskError("test_func", "traceback", cause)

        self.assertIsInstance(error.cause, YRErrorWithCause)
        self.assertIn("Unserializable exception wrapped", str(error.cause))
        self.assertEqual(error.ip, "127.0.0.1")

    def test_default_attributes(self, mock_get_ip):
        error = RayTaskError("func", "tb", Exception())
        self.assertIsNotNone(error.pid)
        self.assertEqual(error.ip, "127.0.0.1")


if __name__ == "__main__":
    unittest.main()