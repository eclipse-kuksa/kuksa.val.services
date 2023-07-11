# /********************************************************************************
# * Copyright (c) 2023 Contributors to the Eclipse Foundation
# *
# * See the NOTICE file(s) distributed with this work for additional
# * information regarding copyright ownership.
# *
# * This program and the accompanying materials are made available under the
# * terms of the Apache License 2.0 which is available at
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * SPDX-License-Identifier: Apache-2.0
# ********************************************************************************/

import asyncio
import logging
import os
import signal
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Dict, Iterator, List

import grpc
from kuksa_client.grpc import Datapoint
from lib.baseservice import BaseService, is_grpc_fatal_error
from lib.behaviorexecutor import BehaviorExecutor
from lib.mockeddatapoint import MockedDataPoint
from lib.datapoint import DataPoint
from lib.loader import PythonDslLoader
from lib.types import Event
from lib.action import AnimationAction
from lib.dsl import _mocked_datapoints, _required_datapoint_paths

SERVICE_NAME = "mock_service"

log = logging.getLogger(SERVICE_NAME)
log.setLevel("INFO")

# Create a file handler and set the log file name
file_handler = logging.FileHandler("mock_service.log")

# Create a log formatter and set the format of log records
formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
file_handler.setFormatter(formatter)

# Add the file handler to the log
log.addHandler(file_handler)

event = threading.Event()

# Set the log level to suppress log messages because we call connect/disconnect of client quite often
logging.getLogger("kuksa_client").setLevel(logging.WARNING)

# Mock Service bind "host:port"
MOCK_ADDRESS = os.getenv("MOCK_ADDR", "0.0.0.0:50053")
VDB_ADDRESS = os.getenv("VDB_ADDRESS", "127.0.0.1:55555")

# Data point events from VDB
EVENT_KEY_ACTUATOR_TARGET = "actuator_target"
EVENT_KEY_VALUE = "value"

log.info(_mocked_datapoints)


class MockService(BaseService):
    """Service implementation which reads custom mocking configuration
    from mock.py and then simulated the programmed behavior of the mocked
    datapoints."""

    def __init__(self, service_address: str, databroker_address: str = VDB_ADDRESS):
        log.info("Initialization ...")
        super().__init__(service_address, SERVICE_NAME, databroker_address)
        self._ids: Dict[str, Any] = dict()
        self._registered = False
        self._last_tick = time.perf_counter()
        self._pending_event_list: List[Event] = list()
        self._mocked_datapoints: Dict[str, MockedDataPoint] = dict()

    # this will work if mock.py is provided
    def on_databroker_connected(self):
        """Callback when a connection to the data broker is established."""
        log.info("Databroker connected!")
        if not self._registered:
            self.check_for_new_mocks(True)
            self._feed_initial_values()

    # this will work on the fly
    def check_for_new_mocks(self, changed=False):
        new_datapoints = set([d["path"] for d in _mocked_datapoints if "path" in d] + _required_datapoint_paths)
        if set(self._mocked_datapoints.keys()) != new_datapoints:
            changed = True
            log.info("Datapoint added/removed")
        else:
            for dict in _mocked_datapoints:
                if dict["behaviors"] != self._mocked_datapoints[dict["path"]].behaviors:
                    changed = True
                    log.info("Behavior added")

        if changed:
            loader_result = PythonDslLoader().load(self._client)
            self._mocked_datapoints = loader_result.mocked_datapoints

            for _, datapoint in self._mocked_datapoints.items():
                datapoint.datapoint.value_listener = self._on_datapoint_updated

            self._behavior_executor = BehaviorExecutor(
                self._mocked_datapoints, self._pending_event_list, self._client
            )
            self._subscribe_to_mocked_datapoints()
            if self._registered is False:
                self._registered = True

    def main_loop(self):
        """Main execution loop which checks if behaviors shall be executed."""
        # wait for datapoints to be registered
        while not self._registered:
            time.sleep(1)
        try:
            while True:
                self.check_for_new_mocks()
                current_tick_time = time.perf_counter()
                delta_time: float = current_tick_time - self._last_tick
                self._behavior_executor.execute(delta_time)

                for _, datapoint in self._mocked_datapoints.items():
                    for behavior in datapoint.behaviors:
                        action = behavior._action
                        if type(action) is AnimationAction:
                            if not action._animator.is_done():
                                action._animator.tick(delta_time)

                self._last_tick = time.perf_counter()

                time.sleep(0.1)
        except Exception as exception:
            log.exception(exception)

    def _on_datapoint_updated(self, datapoint: DataPoint):
        """Callback whenever the value of a datapoint datapoint changes."""
        self._set_datapoint(datapoint.path, datapoint.value)

    def _feed_initial_values(self):
        """Provide initial values of all mocked datapoints to data broker."""
        for mocked in self._mocked_datapoints.values():
            if mocked.datapoint.data_type is not None:
                self._set_datapoint(mocked.datapoint.path, mocked.datapoint.value)

    def _mock_update_request_handler(
        self,
        response_iter: Iterator,
        type,
    ) -> None:
        """Callback when an update event is received from data broker."""
        try:
            for updates in response_iter:
                for path, dp in updates.items():
                    if dp is not None:
                        # else it would register a new event at startup because event with value None would occur
                        if dp.value is not None:
                            raw_value = dp.value
                            self._pending_event_list.append(
                                Event(type, path, raw_value)
                            )
        except Exception as e:
            log.exception(e)
            raise

    def _subscribe_to_mocked_datapoints(self):
        """Subscribe to mocked datapoints."""
        log.info("Subscribing to mocked datapoints...")
        if self._mocked_datapoints:
            response_iter_target = self._client.subscribe_target_values(self._mocked_datapoints)
            response_iter_current = self._client.subscribe_current_values(self._mocked_datapoints)

            self._executor = ThreadPoolExecutor()
            self._executor.submit(self._mock_update_request_handler, response_iter_target, EVENT_KEY_ACTUATOR_TARGET)
            self._executor.submit(self._mock_update_request_handler, response_iter_current, EVENT_KEY_VALUE)

    def _set_datapoint(self, path: str, value: Any):
        """Set the value of a datapoint within databroker."""
        try:
            log.info("Feeding '%s' with value %s", path, value)
            self._client.set_current_values({path: Datapoint(value)})
            # remove events set through set_datapoint
            event_to_remove = None
            for event in self._pending_event_list:
                if "value" == event.name and event.path == path:
                    event_to_remove = event

            if event_to_remove is not None:
                self._pending_event_list.remove(event_to_remove)
        except grpc.RpcError as err:
            log.warning("Feeding %s failed", path, exc_info=True)
            self._connected = is_grpc_fatal_error(err)
            raise err


async def main():
    """Main function"""
    mock_service = MockService(MOCK_ADDRESS)
    mock_service.main_loop()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    log.setLevel(logging.DEBUG)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
