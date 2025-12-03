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
from enum import Enum
from typing import Any, Dict, Optional

from dataclasses import dataclass


class DriverInfo:
    id: str = ""
    node_ip_address: str = ""
    pid: str = ""


class JobType(str, Enum):
    SUBMISSION = "SUBMISSION"
    DRIVER = "DRIVER"


class JobStatus(str, Enum):
    PENDING = "PENDING"
    RUNNING = "RUNNING"
    STOPPED = "STOPPED"
    SUCCEEDED = "SUCCEEDED"
    FAILED = "FAILED"

    def __str__(self) -> str:
        return f"{self.value}"

    def is_terminal(self) -> bool:
        return self.value in {"STOPPED", "SUCCEEDED", "FAILED"}


@dataclass
class JobDetails:
    type: JobType = None
    submission_id: Optional[str] = None
    driver_info: Optional[DriverInfo] = None
    status: JobStatus = None
    entrypoint: str = ""
    message: Optional[str] = None
    error_type: Optional[str] = None
    start_time: Optional[int] = None
    end_time: Optional[int] = None
    metadata: Optional[Dict[str, Any]] = None
    driver_agent_http_address: Optional[str] = None
    driver_node_id: Optional[str] = None
    driver_exit_code: Optional[int] = None

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> "JobDetails":
        driver_info = data.get("driver_info")
        data["driver_info"] = DriverInfo(**driver_info)
        return JobDetails(**data)


