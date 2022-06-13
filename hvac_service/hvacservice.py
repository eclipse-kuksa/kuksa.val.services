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
from concurrent.futures import ThreadPoolExecutor
from threading import Thread

import grpc
from sdv.databroker.v1.collector_pb2 import (
    RegisterDatapointsRequest,
    RegistrationMetadata,
    UpdateDatapointsRequest,
)
from sdv.databroker.v1.collector_pb2_grpc import CollectorStub
from sdv.databroker.v1.types_pb2 import ChangeType, DataType
from sdv.edge.comfort.hvac.v1.hvac_pb2 import (
    SetAcStatusReply,
    SetAcStatusRequest,
    SetTemperatureReply,
    SetTemperatureRequest,
)
from sdv.edge.comfort.hvac.v1.hvac_pb2_grpc import (
    HvacServicer,
    add_HvacServicer_to_server,
)

log = logging.getLogger("hvac_service")
event = threading.Event()

# HVAC Service bind "host:port"
HVAC_ADDRESS = os.getenv("HVAC_ADDR", "0.0.0.0:50052")
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


class HvacService:
    """API to access signals."""

    def __init__(self, hvac_address: str):
        if os.getenv("DAPR_GRPC_PORT") is not None:
            grpc_port = int(os.getenv("DAPR_GRPC_PORT"))
            self._vdb_address = f"127.0.0.1:{grpc_port}"
        else:
            self._vdb_address = VDB_ADDRESS
        self._address = hvac_address
        self._ids = {}
        self._connected = False
        self._registered = False
        self._shutdown = False
        self._databroker_thread = Thread(
            target=self.connect_to_databroker, daemon=True, name="databroker-connector"
        )
        self._databroker_thread.start()
        # self.connect_to_databroker()

    def connect_to_databroker(self) -> None:
        log.info("Connecting to Data Broker [%s]", self._vdb_address)
        if os.getenv("VEHICLEDATABROKER_DAPR_APP_ID") is not None:
            self._metadata = (
                ("dapr-app-id", os.getenv("VEHICLEDATABROKER_DAPR_APP_ID")),
            )
            # give some time for dapr sidecar startup...
            time.sleep(2)
        else:
            self._metadata = None
        self._channel: grpc.Channel = grpc.insecure_channel(self._vdb_address)
        self._stub = CollectorStub(self._channel)
        log.info("Using gRPC metadata: %s", self._metadata)
        self._channel.subscribe(
            lambda connectivity: self.on_broker_connectivity_change(connectivity),
            try_to_connect=False,
        )
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

    def serve(self):
        log.info("Starting HVAC Service on %s", self._address)
        server = grpc.server(ThreadPoolExecutor(max_workers=10))
        _servicer = self._HvacService(self)
        add_HvacServicer_to_server(_servicer, server)
        server.add_insecure_port(self._address)
        server.start()
        server.wait_for_termination()

    async def close(self):
        """Closes runtime gRPC channel."""
        if self._channel:
            await self._channel.close()

    def __enter__(self) -> "HvacService":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        asyncio.run_coroutine_threadsafe(self.close(), asyncio.get_event_loop())

    def register_datapoints(self):
        # Provided via CAN feeder:
        log.info("Try register datapoints")
        self.register(
            "Vehicle.Cabin.IsAirConditioningActive",
            DataType.BOOL,
            ChangeType.ON_CHANGE,
        )
        self.register(
            "Vehicle.Cabin.DesiredAmbientAirTemperature",
            DataType.FLOAT,
            ChangeType.ON_CHANGE,
        )

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
        id = self._ids[name]
        request = UpdateDatapointsRequest()
        request.datapoints[id].float_value = value
        try:
            log.info(" Feeding '%s' with value %s", name, value)
            self._stub.UpdateDatapoints(request, metadata=self._metadata)
        except grpc.RpcError as err:
            log.warning("Feeding %s failed", name, exc_info=True)
            self._connected = is_grpc_fatal_error(err)
            raise err

    def set_bool_datapoint(self, name: str, value: bool):
        id = self._ids[name]
        request = UpdateDatapointsRequest()
        request.datapoints[id].bool_value = value
        log.info(" Feeding '%s' with value %s", name, value)
        try:
            self._stub.UpdateDatapoints(request, metadata=self._metadata)
        except grpc.RpcError as err:
            log.warning("Feeding %s failed", name, exc_info=True)
            self._connected = is_grpc_fatal_error(err)
            raise err

    class _HvacService(HvacServicer):
        def __init__(self, servicer):
            self.servicer: HvacService = servicer

        def SetTemperature(self, request: SetTemperatureRequest, context):
            log.info("* Request to set AC %s", str(request).replace("\n", " "))
            self.servicer.set_float_datapoint(
                "Vehicle.Cabin.DesiredAmbientAirTemperature", request.temperature
            )
            log.info(" Temp updated.\n")
            return SetTemperatureReply()

        def SetAcStatus(self, request: SetAcStatusRequest, context):
            log.info("* Request to set AC %s", str(request).replace("\n", " "))
            self.servicer.set_bool_datapoint(
                "Vehicle.Cabin.IsAirConditioningActive", request.status
            )
            log.info(" AC Status updated.\n")
            return SetAcStatusReply()


async def main():
    """Main function"""
    hvac_service = HvacService(HVAC_ADDRESS)
    hvac_service.serve()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    log.setLevel(logging.DEBUG)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
