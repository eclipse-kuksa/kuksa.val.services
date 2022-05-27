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
from concurrent.futures import ThreadPoolExecutor

import grpc
from sdv.databroker.v1.collector_pb2 import (
    RegisterDatapointsRequest,
    RegistrationMetadata,
    UpdateDatapointsRequest,
)
from sdv.databroker.v1.collector_pb2_grpc import CollectorStub
from sdv.databroker.v1.types_pb2 import ChangeType, DataType
from sdv.edge.comfort.hvac.v1.hvac_pb2 import SetAcStatusReply, SetTemperatureReply
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


class HvacService:
    """API to access signals."""

    def __init__(self, hvac_address: str):
        if os.getenv("DAPR_GRPC_PORT") is not None:
            grpc_port = int(os.getenv("DAPR_GRPC_PORT"))
            self._vdb_address = f"127.0.0.1:{grpc_port}"
        else:
            self._vdb_address = VDB_ADDRESS

        self._address = hvac_address

        log.info("Connecting to Data Broker using %s", self._vdb_address)
        self._channel = grpc.insecure_channel(self._vdb_address)
        self._stub = CollectorStub(self._channel)
        if os.getenv("VEHICLEDATABROKER_DAPR_APP_ID") is not None:
            self._metadata = (
                ("dapr-app-id", os.getenv("VEHICLEDATABROKER_DAPR_APP_ID")),
            )
        else:
            self._metadata = None
        log.info("Using gRPC metadata: %s", self._metadata)
        self._ids = {}
        self._connected = False
        self._registered = False
        self._channel.subscribe(
            lambda connectivity: self.on_broker_connectivity_change(connectivity),
            try_to_connect=False,
        )

    def serve(self):
        log.info("Starting HVAC Service on %s", self._address)
        server = grpc.server(ThreadPoolExecutor(max_workers=10))
        _servicer = self._HvacService(self)
        add_HvacServicer_to_server(_servicer, server)
        server.add_insecure_port(self._address)
        server.start()
        server.wait_for_termination()

    def on_broker_connectivity_change(self, connectivity):
        log.info("[%s] Connectivity changed to: %s", self._vdb_address, connectivity)
        if (
            connectivity == grpc.ChannelConnectivity.READY
            or connectivity == grpc.ChannelConnectivity.IDLE
        ):
            # Can change between READY and IDLE. Only act if coming from
            # unconnected state
            if not self._connected:
                log.info("Connected to data broker: %s", self._vdb_address)
                try:
                    self.register_datapoints()
                    log.info("Datapoints registered")
                    self._registered = True
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
        log.info(" Feeding '%s' with value %s", name, value)
        self._stub.UpdateDatapoints(request, metadata=self._metadata)

    def set_bool_datapoint(self, name: str, value: bool):
        id = self._ids[name]
        request = UpdateDatapointsRequest()
        request.datapoints[id].bool_value = value
        log.info(" Feeding '%s' with value %s", name, value)
        self._stub.UpdateDatapoints(request, metadata=self._metadata)

    class _HvacService(HvacServicer):
        def __init__(self, servicer):
            self.servicer = servicer

        def SetTemperature(self, request, context):
            log.info("* Request to set AC %s", str(request).replace("\n", " "))
            self.servicer.set_float_datapoint(
                "Vehicle.Cabin.DesiredAmbientAirTemperature", request.temperature
            )
            log.info(" Temp updated.\n")
            return SetTemperatureReply()

        def SetAcStatus(self, request, context):
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
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
