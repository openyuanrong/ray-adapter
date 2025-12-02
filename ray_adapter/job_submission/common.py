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

from typing import Any, Dict, Optional, Union

from dataclasses import dataclass


@dataclass
class JoSubmitRequest:
    entrypoint: str
    submission_id: Optional[Dict[str, Any]] = None
    runtime_env: Optional[Dict[str, Any]] = None
    metadata: Optional[Dict[str, str]] = None
    entrypoint_num_cpus: Optional[Union[int, float]] = None
    entrypoint_num_gpus: Optional[Union[int, float]] = None
    entrypoint_memory: Optional[int] = None

    def __post_init__(self):
        if not isinstance(self.entrypoint, str):
            raise TypeError(f"entrypoint must be a string, got {type(self.entrypoint)}")

        if self.submission_id is not None and not isinstance(self.submission_id, str):
            raise TypeError(
                "submission_id must be a string if provided,"
                f"got {type(self.submission_id)}"
            )

        if self.runtime_env is not None:
            if not isinstance(self.runtime_env, dict):
                raise TypeError(
                    f"runtime_env must be a dict, got {type(self.runtime_env)}"
                )
            for k in self.runtime_env.keys():
                if not isinstance(k, str):
                    raise TypeError(
                        f"runtime_env keys must be a strings, got {type(k)}"

                    )
        if self.metadata is not None:
            if not isinstance(self.metadata, dict):
                raise TypeError(f"metadata must be a dict, got {type(self.metadata)}")
            for k in self.metadata.keys():
                if not isinstance(k, str):
                    raise TypeError(f"metadata keys must be strings, got {type(k)}")
            for v in self.metadata.values():
                if not isinstance(v ,str):
                    raise TypeError(
                        f"metadata values must be strings, got {type(v)}"
                    )


@dataclass
class JobSubmitResponse:
    submission_id: str


@dataclass
class JobStopResponse:
    stopped: bool


@dataclass
class JobDeleteResponse:
    deleted: bool