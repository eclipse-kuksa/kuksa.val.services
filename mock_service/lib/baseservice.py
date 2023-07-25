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

import grpc
from kuksa_client.grpc import VSSClient

log = logging.getLogger("base_service")

# VehicleDataBroker address, overridden if "DAPR_GRPC_PORT" is set in environment
VDB_ADDRESS = os.getenv("VDB_ADDRESS", "127.0.0.1:55555")
VDB_IP = VDB_ADDRESS.split(':')[0]
VDB_PORT = VDB_ADDRESS.split(':')[1]


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
        self._address = service_address
        self._service_name = service_name
        self._connected = False
        self._shutdown = False
        self._client = VSSClient(VDB_IP, VDB_PORT)
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
        self._client.connect()
        self._connected = True
        self.on_databroker_connected()

        self._run()

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

    async def close(self):
        """Closes runtime gRPC channel."""
        self._client.disconnect()

    def __enter__(self) -> "BaseService":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        asyncio.run_coroutine_threadsafe(self.close(), asyncio.get_event_loop())
