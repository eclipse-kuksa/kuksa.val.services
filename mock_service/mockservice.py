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
from kuksa.val.v1.types_pb2 import Field
from kuksa.val.v1.val_pb2 import SubscribeEntry, SubscribeRequest, SubscribeResponse
from lib.animator import Animator
from lib.baseservice import BaseService, is_grpc_fatal_error
from lib.behavior import Behavior, BehaviorExecutor
from lib.datapoint import MockedDataPoint
from lib.loader import PythonDslLoader
from lib.types import Event
from kuksa_client.grpc import DataType, Metadata, Datapoint

SERVICE_NAME = "mock_service"

log = logging.getLogger(SERVICE_NAME)
event = threading.Event()

# Mock Service bind "host:port"
MOCK_ADDRESS = os.getenv("MOCK_ADDR", "0.0.0.0:50053")

# Data point events from VDB
EVENT_KEY_ACTUATOR_TARGET = "actuator_target"
EVENT_KEY_VALUE = "value"


class MockService(BaseService):
    """Service implementation which reads custom mocking configuration
    from mock.py and then simulated the programmed behavior of the mocked
    datapoints."""

    def __init__(self, service_address: str):
        super().__init__(service_address, SERVICE_NAME)
        self._ids: Dict[str, Any] = dict()
        self._registered = False
        self._last_tick = time.perf_counter()
        self._pending_event_list: List[Event] = list()
        self._animators: List[Animator] = list()
        self._vdb_metadata: Dict[str, Metadata] = dict()
        self._mocked_datapoints: Dict[str, MockedDataPoint] = dict()
        self._behaviors: List[Behavior] = list()

    def on_databroker_connected(self):
        """Callback when a connection to the data broker is established."""
        if not self._registered:
            loader_result = PythonDslLoader().load(self._client)
            self._mocked_datapoints = loader_result.mocked_datapoints
            for _, datapoint in self._mocked_datapoints.items():
                datapoint.value_listener = self._on_datapoint_updated
            self._behaviors = loader_result.behavior_dict

            self._behavior_executor = BehaviorExecutor(
                self._mocked_datapoints, self._behaviors, self._pending_event_list
            )
            self._subscribe_to_mocked_datapoints()
            self._feed_initial_values()
            self._registered = True

    def main_loop(self):
        """Main execution loop which checks if behaviors shall be executed."""
        # wait for datapoints to be registered
        while not self._registered:
            time.sleep(1)

        try:
            while True:
                current_tick_time = time.perf_counter()
                delta_time: float = current_tick_time - self._last_tick
                self._behavior_executor.execute(delta_time, self._animators)

                remove_animators: List[Animator] = list()
                for animator in self._animators:
                    if not animator.is_done():
                        animator.tick(delta_time)
                    else:
                        remove_animators.append(animator)

                for animator in remove_animators:
                    self._animators.remove(animator)

                self._last_tick = time.perf_counter()
                time.sleep(0.1)
        except Exception as exception:
            log.exception(exception)

    def _on_datapoint_updated(self, datapoint: MockedDataPoint):
        """Callback whenever the value of a mocked datapoint changes."""
        self._set_datapoint(datapoint.path, datapoint.data_type, datapoint.value)

    def _feed_initial_values(self):
        """Provide initial values of all mocked datapoints to data broker."""
        for datapoint in self._mocked_datapoints.values():
            if datapoint.data_type is not None:
                self._set_datapoint(
                    datapoint.path, datapoint.data_type, datapoint.value
                )

    def _mock_update_request_handler(
        self, response_iterator_current: Iterator, response_iterator_target: Iterator,
    ) -> None:
        """Callback when an update event is received from data broker."""
        try:
            for updates in response_iterator_target:
                for path, dp in updates.items():
                    if dp is not None:
                        raw_value = dp.value
                    else:
                        raw_value = None
                    self._pending_event_list.append(
                        Event(
                            EVENT_KEY_ACTUATOR_TARGET, path, raw_value
                        )
                    )
            for updates in response_iterator_current:
                for path, dp in updates.items():
                    if dp is not None:
                        raw_value = dp.value
                    else:
                        raw_value = None
                    self._pending_event_list.append(
                        Event(
                            EVENT_KEY_VALUE, path, raw_value
                        )
                    )
        except Exception as e:
            log.exception(e)
            raise

    def _subscribe_to_mocked_datapoints(self):
        """Subscribe to mocked datapoints."""
        log.info("Subscribing to mocked datapoints...")

        response_iter_current = self._client.subscribe_current_values(self._mocked_datapoints)
        response_iter_target = self._client.subscribe_target_values(self._mocked_datapoints)

        self._executor = ThreadPoolExecutor()
        self._executor.submit(self._mock_update_request_handler, response_iter_current, response_iter_target)

    def _set_datapoint(self, name: str, data_type: DataType, value: Any):
        """Set the value of a datapoint within databroker."""
        try:
            log.info("Feeding '%s' with value %s", name, value)
            self._client.set_current_values({name: Datapoint(value)})
        except grpc.RpcError as err:
            log.warning("Feeding %s failed", name, exc_info=True)
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
