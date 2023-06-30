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
import time
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from threading import Thread
from typing import Optional, Tuple

import grpc
from kuksa.val.v1.val_pb2_grpc import VALStub
from sdv.databroker.v1.broker_pb2_grpc import BrokerStub
from sdv.databroker.v1.collector_pb2_grpc import CollectorStub

log = logging.getLogger("base_service")

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


class BaseService(ABC):
    """Base Service implementation which connects to the databroker."""

    def __init__(self, service_address: str, service_name: str):
        super().__init__()

        if os.getenv("DAPR_GRPC_PORT") is not None:
            grpc_port = os.getenv("DAPR_GRPC_PORT")
            self._vdb_address = f"127.0.0.1:{grpc_port}"
        else:
            self._vdb_address = VDB_ADDRESS
        self._metadata: Optional[Tuple[Tuple[str, Optional[str]]]] = None
        self._address = service_address
        self._service_name = service_name
        self._connected = False
        self._shutdown = False
        self._channel = None
        self._stub = None
        self._stub_val = None
        self._stub_broker: Optional[BrokerStub] = None
        self._databroker_thread = Thread(
            target=self._connect_to_databroker, daemon=True, name="databroker-connector"
        )
        self._databroker_thread.start()

    def _connect_to_databroker(self) -> None:
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
        self._stub_val = VALStub(self._channel)
        self._stub_broker = BrokerStub(self._channel)

        log.info("Using gRPC metadata: %s", self._metadata)
        self._channel.subscribe(
            self._on_broker_connectivity_change,
            try_to_connect=False,
        )
        self._run()

    def _on_broker_connectivity_change(self, connectivity):
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
                    self.on_databroker_connected()
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

    def _run(self):
        while self._shutdown is False:
            if not self._connected:
                time.sleep(0.2)
                continue
            else:
                # TODO: check if dapr grpc proxy has active connection(e.g. send last temp value)
                time.sleep(1)

    @abstractmethod
    def on_databroker_connected(self):
        pass

    def serve(self):
        log.info(f"Starting {self._service_name}")
        server = grpc.server(ThreadPoolExecutor(max_workers=10))
        server.add_insecure_port(self._address)
        server.start()
        server.wait_for_termination()

    async def close(self):
        """Closes runtime gRPC channel."""
        if self._channel:
            await self._channel.close()

    def __enter__(self) -> "BaseService":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        asyncio.run_coroutine_threadsafe(self.close(), asyncio.get_event_loop())
