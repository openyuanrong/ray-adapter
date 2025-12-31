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
import yr
from ray_adapter._private.services import get_node_ip_address, list_named_actors


class TestNodeIp(unittest.TestCase):
    @patch('yr.apis.get_node_ip_address')
    def test_get_node_ip_address(self, mock_get_node_ip):
        mock_get_node_ip.return_value = "172.0.0.1"
        res = get_node_ip_address()
        self.assertEqual(res, "172.0.0.1")


class TestListName(unittest.TestCase):
    @patch('yr.apis.list_named_instances')
    def test_list_named_actors(self, mock_list_name):
        mock_list_name.return_value = ["12", "21", "32"]
        res = list_named_actors()
        self.assertEqual(res, mock_list_name.return_value)


if __name__ == "__main__":
    unittest.main()
