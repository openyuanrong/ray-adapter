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
import json
import socket
import time
import unittest
import dataclasses
from multiprocessing import Process

import aiohttp
from aiohttp.web import Request, Response

from ray_adapter.job_submission.sdk import (JoSubmitRequest, JobSubmitResponse, JobSubmissionClient,
                                            JobDetails, JobStopResponse, JobDeleteResponse)
from ray_adapter.job_submission.utils import strip_keys_with_value_none
from ray_adapter.job_submission.model import DriverInfo, JobType, JobStatus


def get_mock_job_details(submission_id: str) -> JobDetails:
    driver_info = DriverInfo(id="driver", node_ip_address="127.0.0.1", pid="22222")
    detail = JobDetails(type=JobType.SUBMISSION,
                        submission_id=submission_id, driver_info=driver_info,
                        status=JobStatus.RUNNING,
                        entrypoint="echo 3", message="no thing", error_type="no thing", start_time=0, end_time=1,
                        metadata={"key": "value"}, runtime_env={"key": 2}, driver_agent_http_address="0",
                        driver_node_id="node_id", driver_exit_code=0)
    return detail


async def list_jobs(req: Request) -> Response:
    detail = get_mock_job_details("id")
    return Response(text=json.dumps([dataclasses.asdict(detail)]), content_type='application/json')


async def get_job_info(req: Request) -> Response:
    submission_id = req.match_info["submission_id"]
    detail = get_mock_job_details(submission_id)
    return Response(text=json.dumps(dataclasses.asdict(detail)), content_type='application/json')


class MockDashboardServer:
    def __init__(self, host="127.0.0.1", port=9080):
        self.host = host
        self.port = port
        self.app = aiohttp.web.Application()

    async def submit_job(self, req: Request) -> Response:
        json_data = strip_keys_with_value_none(await req.json())
        submit_request = JoSubmitRequest(**json_data)
        submission_id = submit_request.submission_id
        resp = JobSubmitResponse(submission_id=submission_id)
        return Response(
            text=json.dumps(dataclasses.asdict(resp)),
            content_type='application/json',
            status=aiohttp.web.HTTPOk.status_code

        )

    async def stop_job(self, req: Request) -> Response:
        resp = JobStopResponse(True)
        return Response(text=json.dumps(resp.stopped), content_type='application/json')

    async def delete_job(self, req: Request) -> Response:
        resp = JobDeleteResponse(True)
        return Response(text=json.dumps(resp.deleted), content_type='application/json')

    def configure_routes(self):
        self.app.add_routes([aiohttp.web.post('/api/jobs/', self.submit_job),
                             aiohttp.web.post('/api/jobs/{submission_id}/stop', self.stop_job),
                             aiohttp.web.delete('/api/jobs/{submission_id}', self.delete_job),
                             aiohttp.web.get('/api/jobs/{submission_id}', get_job_info),
                             aiohttp.web.get('/api/jobs/', list_jobs)
                             ],)

    def run(self):
        self.configure_routes()
        aiohttp.web.run_app(self.app, host=self.host, port=self.port)


def is_port_open(host, port, timeout=1):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(timeout)
        try:
            s.connect((host, port))
            return True
        except (socket.timeout, socket.error):
            return False


class TestJobs(unittest.TestCase):
    submission_id: str = "test_job123"

    @classmethod
    def setUpClass(cls):
        server = MockDashboardServer()
        cls.process = Process(target=server.run)
        cls.process.start()
        while not is_port_open("127.0.0.1", 9080):
            time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        cls.process.join()

    def test_submit_jobs(self):
        client = JobSubmissionClient()
        submission_id = client.submit_job(entrypoint="echo 3", submission_id=self.submission_id)
        self.assertEqual(submission_id, self.submission_id)

    def test_stop_jobs(self):
        client = JobSubmissionClient()
        result = client.stop_job(self.submission_id)
        self.assertTrue(result)

    def test_get_job_info(self):
        client = JobSubmissionClient()
        result = client.get_job_info("test_job123")
        self.assertEqual(result.submission_id, self.submission_id)
        self.assertEqual(result.type, JobType.SUBMISSION)
        self.assertEqual(result.status, JobStatus.RUNNING)
        self.assertEqual(result.driver_info.node_ip_address, "127.0.0.1")
        self.assertEqual(result.driver_node_id, "node_id")

    def test_list_jobs(self):
        client = JobSubmissionClient()
        result = client.list_jobs()
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].driver_info.node_ip_address, "127.0.0.1")

    def test_init_client_with_address(self):
        client = JobSubmissionClient("http://127.0.0.1:9080")
        submission_id = client.submit_job(entrypoint="echo 3", submission_id=self.submission_id)
        self.assertEqual(submission_id, self.submission_id)

    def test_delete_job(self):
        client = JobSubmissionClient()
        submission_id = "test_delete_job_123"
        client.submit_job(entrypoint="sleep 1", submission_id=submission_id)
        deleted = client.delete_job(submission_id)
        self.assertTrue(deleted)

if __name__ == "__main__":
    unittest.main()
