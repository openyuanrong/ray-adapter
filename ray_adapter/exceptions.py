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
from typing import Optional
from yr.exception import YRError
from ray_adapter._private.services import get_node_ip_address

logger = logging.getLogger(__name__)


class GetTimeoutError(YRError):
    """
    Indicates that a call to a worker or task timed out.
    """


class YRErrorWithCause(YRError):
    """
    A subclass of YRError that stores the original cause of the error.
    """

    def __int__(self, message: str, cause: Optional[Exception] = None):
        super().__int__(message)
        self.cause = cause


class RayTaskError(YRError):
    """
    Indicates that a task threw an exceptions during execution.
    """

    def __init__(
            self,
            function_name: str,
            traceback_str: str,
            cause: Exception,
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
        Handle YRInvokeError dynamic subclasses by reconstructing standard exceptions.
        """
        candidate = cause
        qualname = type(cause).__qualname__

        if qualname.startwith('YRInvokeError('):
            error_msg = str(cause)
            try:
                inner_name = qualname.split('(')[1].rstrip(')')
            except IndexError:
                inner_name = 'Exception'
            if inner_name == 'ValueError':
                candidate = ValueError(error_msg)
            if inner_name == 'TypeError':
                candidate = TypeError(error_msg)
            if inner_name == 'RuntimeError':
                candidate = RuntimeError(error_msg)
            else:
                candidate = Exception(error_msg)
        try:
            pickle.dumps(candidate)
            self.cause = candidate
        except (pickle.PicklingError, TypeError) as e:
            logger.warning(
                f"The cause {type(candidate)} is not serializable: {e}."
                f"Falling back to YRErrorWithCause."
            )
            self.cause = YRErrorWithCause(
                f"Unserializable exception wrapped: {type(candidate)}",
                cause=candidate
            )

        @staticmehtod
        def ray_task_wrap(func):
            """
            Decorator to wrap a remote task so that any exception is converted to RayTaskError.
            """
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                raise RayTaskError(
                    function_name=func.__name__,
                    traceback_str=str(e),
                    cause=e
                ) from e
        return wrapper

