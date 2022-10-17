# /********************************************************************************
# * Copyright (c) 2022 Contributors to the Eclipse Foundation
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
import sys
from concurrent.futures import ThreadPoolExecutor
from threading import Thread
from typing import Any, Callable, Dict, List, Mapping, Optional, Tuple

import grpc

# Broker for receiving Vehicle Data from Data Broker
from sdv.databroker.v1.broker_pb2 import (
    GetDatapointsRequest,
    GetMetadataRequest,
    SubscribeRequest,
)
from sdv.databroker.v1.broker_pb2_grpc import BrokerStub

# Collector for sending Vehicle Data to Data Broker
from sdv.databroker.v1.collector_pb2 import (
    RegisterDatapointsRequest,
    RegistrationMetadata,
    UpdateDatapointsRequest,
)
from sdv.databroker.v1.collector_pb2_grpc import CollectorStub
from sdv.databroker.v1.types_pb2 import ChangeType, DataType, Datapoint

log = logging.getLogger("carsim")
event = threading.Event()

# VehicleDataBroker address, overridden if "DAPR_GRPC_PORT" is set in environment
VDB_ADDRESS = os.getenv("VDB_ADDRESS", "127.0.0.1:55555")

def is_grpc_fatal_error(e: grpc.RpcError) -> bool:
    if (
        e.code() == grpc.StatusCode.UNAVAILABLE
        or e.code() == grpc.StatusCode.UNKNOWN
        or e.code() == grpc.StatusCode.UNAUTHENTICATED
        or e.code() == grpc.StatusCode.INTERNAL
    ):
        log.error("Feeding aborted due to RpcError(%s, '%s')", e.code(), e.details())
        return True
    else:
        log.warning("Unhandled RpcError(%s, '%s')", e.code(), e.details())
        return False

def sigterm_handler(_signo, _stack_frame):
    # Raises SystemExit(0):
    log.info("Gracefully shutting down...")
    sys.exit(0)

class CarSim:
    """API to access signals."""

    def __init__(self):
        self._vdb_address = VDB_ADDRESS
        self._metadata: Optional[Tuple[Tuple[str, Optional[str]]]] = None
        self._ids: Dict[str, Any] = {}
        self._connected = False
        self._registered = False
        self._shutdown = False

        # Some vehicle data
        self._vehicle_speed = -1

        self._console_thread = Thread(
            target=self.print_console, daemon=True, name="console-printer"
        )
        self._console_thread.start()

        self._databroker_thread = Thread(
            target=self.connect_to_databroker, daemon=True, name="databroker-connector"
        )
        self._databroker_thread.start()
        # self.connect_to_databroker()

    def print_console(self) -> None:
        log.info("Starting console logger")
        while self._shutdown is False:
            log.info("Vehicle.Speed: %s", self._vehicle_speed)
            time.sleep(1)
    
    def do_connectivity(self, connectivity):
        self.on_broker_connectivity_change(connectivity)
    
    def connect_to_databroker(self) -> None:
        log.info("Connecting to Data Broker [%s]", self._vdb_address)
        self._metadata = None
        self._channel: grpc.Channel = grpc.insecure_channel(self._vdb_address)
        self._stub = CollectorStub(self._channel)

        self._broker_stub = BrokerStub(self._channel)
        log.info("Using gRPC metadata: %s", self._metadata)     
        self._channel.subscribe(self.do_connectivity,try_to_connect=False)
        self._run()

    def on_broker_connectivity_change(self, connectivity):
        log.info("[%s] Connectivity changed to: %s", self._vdb_address, connectivity)
        if (
            connectivity == grpc.ChannelConnectivity.READY
            or connectivity == grpc.ChannelConnectivity.IDLE
        ):
            # Can change between READY and IDLE. Only act if coming from
            # unconnected state
            if not self._connected:
                log.info("Connected to data broker")
                try:
                    self.register_datapoints()
                    log.info("datapoints are registered.")
                    self._registered = True
                except grpc.RpcError as err:
                    log.error("Failed to register datapoints")
                    is_grpc_fatal_error(err)
                    # log.error("Failed to register datapoints", exc_info=True)
                except Exception:
                    log.error("Failed to register datapoints", exc_info=True)
                self._connected = True
        else:
            if self._connected:
                log.info("Disconnected from data broker")
            else:
                if connectivity == grpc.ChannelConnectivity.CONNECTING:
                    log.info("Trying to connect to data broker")
            self._connected = False
            self._registered = False

    def _run(self):
        while self._shutdown is False:
            if not self._connected:
                time.sleep(0.2)
                continue
            elif not self._registered:
                try:
                    log.debug("Try to register datapoints")
                    self.register_datapoints()
                    self._registered = True
                except grpc.RpcError as err:
                    is_grpc_fatal_error(err)
                    log.debug("Failed to register datapoints", exc_info=True)
                    time.sleep(3)
                except Exception:
                    log.error("Failed to register datapoints", exc_info=True)
                    time.sleep(1)
                    continue
            else:
                # TODO: check if dapr grpc proxy has active connection(e.g. send last temp value)
                time.sleep(1)

    async def close(self):
        """Closes runtime gRPC channel."""
        if self._channel:
            await self._channel.close()

    def register_datapoints(self):
        # Provided via CAN feeder:
        log.info("Try register datapoints")
        
        # Input:
        # Vehicle.Throttle.PedalPosition
        self.subscribe("Vehicle.Throttle.PedalPosition", self.on_throttle_pedalposition, 1)
        self.subscribe("Vehicle.Breaks.PedalPosition", self.on_break_pedalposition, 1)
        self.subscribe("Vehicle.SteeringWheel.Angle", self.on_steeringwheel_angle, 1)
        
        # Output:
        # Vehicle.Acceleration
        # Vehicle.Speed
        self.register("Vehicle.Speed", DataType.FLOAT, ChangeType.CONTINUOUS)
        self.register("Vehicle.Acceleration", DataType.FLOAT, ChangeType.CONTINUOUS)     

    def on_throttle_pedalposition(self, data):
        log.info("on_throttle_pedalposition")

    def on_break_pedalposition(self, data):
        log.info("on_break_pedalposition")

    def on_steeringwheel_angle(self, data):
        log.info("on_steeringwheel_angle")

    def subscribe(
        self,
        query: str,
        sub_callback: Callable[[str, Datapoint], None],
        timeout: Optional[int] = None,
    ) -> None:
        try:
            request = SubscribeRequest(query=query)
            log.info("broker.Subscribe('{}')".format(query))
            response = self._broker_stub.Subscribe(
                request, metadata=self._grpc_metadata, timeout=timeout
            )
            # NOTE:
            # 'async for' before iteration is crucial here with aio.channel!
            for subscribe_reply in response:
                log.debug("Streaming SubscribeReply %s", subscribe_reply)
                """ from broker.proto:
                message SubscribeReply {
                    // Contains the fields specified by the query.
                    // If a requested data point value is not available, the corresponding
                    // Datapoint will have it's respective failure value set.
                    map<string, databroker.v1.Datapoint> fields = 1;
                }"""
                if not hasattr(subscribe_reply, "fields"):
                    raise Exception("Missing 'fields' in {}".format(subscribe_reply))

                log.debug("SubscribeReply.{}".format(subscribe_reply.fields))
                for name in subscribe_reply.fields:
                    dp = subscribe_reply.fields[name]
                    try:
                        log.debug("Calling sub_callback({}, dp:{})".format(name, dp))
                        sub_callback(name, dp)
                    except Exception:
                        log.exception("sub_callback() error", exc_info=True)
                        pass
            log.debug("Streaming SubscribeReply done...")

        except grpc.RpcError as e:
            if (
                e.code() == grpc.StatusCode.DEADLINE_EXCEEDED
            ):  # expected code if we used timeout, just stop subscription
                log.debug("Exitting after timeout: {}".format(timeout))
            else:
                log.error(
                    "broker.Subscribe({}) failed!\n --> {}".format(
                        query, self.__get_grpc_error(e)
                    )
                )
                raise e
        except Exception:
            log.exception("broker.Subscribe() error", exc_info=True)


    def register(self, name, data_type, change_type):
        self._register(name, data_type, change_type)

    def _register(self, name, data_type, change_type):
        request = RegisterDatapointsRequest()
        registration_metadata = RegistrationMetadata()
        registration_metadata.name = name
        registration_metadata.data_type = data_type
        registration_metadata.description = ""
        registration_metadata.change_type = change_type
        request.list.append(registration_metadata)
        response = self._stub.RegisterDatapoints(request, metadata=self._metadata)
        self._ids[name] = response.results[name]

    def set_float_datapoint(self, name: str, value: float):
        _id = self._ids[name]
        request = UpdateDatapointsRequest()
        request.datapoints[_id].float_value = value
        try:
            log.info(" Feeding '%s' with value %s", name, value)
            self._stub.UpdateDatapoints(request, metadata=self._metadata)
        except grpc.RpcError as err:
            log.warning("Feeding %s failed", name, exc_info=True)
            self._connected = is_grpc_fatal_error(err)
            raise err


async def main():
    """Main function"""
    carsim = CarSim()
    await asyncio.sleep(10)
    

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    log.setLevel(logging.DEBUG)
    signal.signal(signal.SIGTERM, sigterm_handler)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.add_signal_handler(signal.SIGTERM, sigterm_handler)
    try:
        LOOP.run_until_complete(main())
    except KeyboardInterrupt:
        sigterm_handler()
    LOOP.close()
