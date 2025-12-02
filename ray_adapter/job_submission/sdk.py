#!/usr/bin/env python3
# coding=UTF-8
# Copray_adapteright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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
from typing import Any, Dict, List, Optional, Union

import dataclasses
import requests

from ray_adapter.job_submission.common import JoSubmitRequest, JobSubmitResponse, JobDeleteResponse, JobStopResponse
from ray_adapter.job_submission.model import JobDetails
from ray_adapter.job_submission.utils import strip_keys_with_value_none


class JobSubmissionClient:
    def __init__(
            self,
            address: Optional[str] = None,
            create_cluster_if_needed = False,
            cookies: Optional[Dict[str, Any]] = None,
            metadata: Optional[Dict[str, Any]] = None,
            headers: Optional[Dict[str ,Any]] = None,
            verify: Optional[Union[str, bool]] = True,
    ):
        self._address = address or "http://localhost:9080"
        self._cookies = cookies
        self._default_metadata = metadata or {}
        self._headers = headers
        self._verify = verify
        self._address = os.environ.get("ray_adapter_DASHBOARD_ADDRESS",self._address)

    @staticmethod
    def _raise_error(r: requests.Response):
        raise RuntimeError(
            f"Request failed with status code {r.status_code}:{r.text}."
        )

    def submit_job(
            self,
            *,
            entrypoint: str,
            job_id: Optional[str] = None,
            runtime_env: Optional[Dict[str, Any]] = None,
            metadata: Optional[Dict[str, Any]] = None,
            submission_id: Optional[str] = None,
            entrypoint_num_cpus: Optional[Union[int, float]] = None,
            entrypoint_num_gpus: Optional[Union[int, float]] = None,
            entrypoint_memory: Optional[int] = None,
            entrypoint_resources: Optional[Dict[str, float]] = None,
    ) -> str:
        metadata = metadata or {}
        metadata.update(self._default_metadata)
        req = JobSubmitRequest(entrypoint=entrypoint, submission_id=submission_id, runtime_env=runtime_env,
                               metadata=metadata, entrypoint_num_cpus=entrypoint_num_cpus,
                               entrypoint_num_gpus=entrypoint_num_gpus, entrypoint_memory=entrypoint_memory,
                               entrypoint_resources=entrypoint_resources)
        json_data = strip_keys_with_value_none(dataclasses.asdict(req))
        r = self._do_request("POST", "/api/jobs/", json_data=json_data)
        if r.status_code ==200:
            return JobSubmitResponse(**r.json()).submission_id
        self._raise_error(r)

    def stop_job(self, job_id: str, ) -> bool:
        r = self._do_request("POST", f"/api/jobs/{job_id}/stop")
        if r.status_code == 200:
            return JobStopResponse(r.json()).stopped
        self._raise_error(r)

    def delete_job(self, job_id: str) -> bool:
        r = self._do_request("DELETE", f"/api/jobs/{job_id}")
        if r.status_code == 200:
            return JobDeleteResponse(r.json()).deleted
        self._raise_error(r)

    def get_job_info(self, job_id: str) -> JobDetails:
        r = self._do_request("GET", f"/api/jobs/{job_id}")
        if r.status_code == 200:
            return JobDetails.from_dict(r.json())

    def list_jobs(self) -> List[JobDetails]:
        r = self._do_request("GET", f"/api/jobs/")
        if r.status_code == 200:
            jobs_info_json = r.json()
            jobs_info = [JobDetails.from_dict(job_info_json) for job_info_json in jobs_info_json]
            return jobs_info
        self._raise_error(r)

    def _do_request(self, method:str, endpoint: str, *, data: Optional[bytes] = None, json_data: Optional[Dict] = None,
                    **kwargs, ) -> requests.Response:
        url = self._address + endpoint
        return requests.request(method, url, cookies=self._cookies, data=data, json=json_data, headers=self._headers,
                        verify=self._verify, **kwargs, )


