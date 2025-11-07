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
import pickle
import logging
from ray_adapter._private.services import get_node_ip_address


class YRError(Exception):
    """
    Base class for all custom exceptions in the YR module.
    This is a base class and should not be instantiated directly.
    """


class GetTimeoutError(YRError):
    """
    Indicates that a call to a worker or task timed out.
    """


class RayTaskError(YRError):
    """
    Indicates that a task threw an exceptions during execution.
    """

    def __init__(
            self,
            function_name: str,
            traceback_str: str,
            cause: Excaptions,
            proctitle: str = None,
            pid: int = None,
            ip: str = None,
            actor_repr: str = None,
            actor_id: str = None,


    ):
        message = f"Funcion: {function_name}, Error: {traceback_str}, Cause: {cause}"
        super().__init__(message)
        self.function_name = function_name
        self.traceback_str = traceback_str
        self.proctitle = proctitle
        self.pid = pid or os.getpid()
        self.ip = ip or get_node_ip_address()
        self.actor_repr = actor_repr
        self.actor_id = actor_id or os.getenv("INSTANCE_ID")
        self._set_cause(cause)
        self.args = (function_name, traceback_str, self.cause, proctitle, pid, ip)

    def _get_proctitle(self, proctitle: str = None) -> str:
        """
        Get the process title.
        """
        return proctitle or getattr(os, "getproctitle", lambda: "yr-task")()

    def _set_cause(self, cause: Exception):
        """
        Set the cause of the error, handling pickling issues.
        """
        try:
            pickle.dumps(cause)
            self.cause = cause
        except (pickle.PicklingError, TypeError) as e:
            err_msg = (
                f"The original cause of RayTaskError ({type(cause)})"
                f"isn't serializable: {e}. Overwriting the cause to a YRError"
            )
            logger.warning(err_msg)
            self.cause = YRError(err_msg)