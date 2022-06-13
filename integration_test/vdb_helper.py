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

# Disable name checks due to proto generated classes
# pylint: disable=C0103

import asyncio
import logging
import os
import signal
from typing import Callable, Dict, Mapping, Optional

import grpc
from gen_proto.sdv.databroker.v1.broker_pb2 import (
    GetDatapointsRequest,
    GetMetadataRequest,
    SubscribeRequest,
)
from gen_proto.sdv.databroker.v1.broker_pb2_grpc import BrokerStub
from gen_proto.sdv.databroker.v1.collector_pb2 import (
    RegisterDatapointsRequest,
    RegistrationMetadata,
    UpdateDatapointsRequest,
)
from gen_proto.sdv.databroker.v1.collector_pb2_grpc import CollectorStub
from gen_proto.sdv.databroker.v1.types_pb2 import ChangeType, Datapoint, DataType

logger = logging.getLogger(__name__)


class VDBHelper:
    """
    VDBHelper wraps collector and broker APIs of the Vehicle Data Broker.
    """

    def __init__(self, address: str) -> None:

        if os.getenv("DAPR_GRPC_PORT") is not None:
            self._address = "127.0.0.1:{}".format(os.getenv("DAPR_GRPC_PORT"))
        else:
            self._address = address

        logger.info("VDBHelper connecting to {}".format(self._address))
        # WARNING: always await grpc response!
        self._channel = grpc.aio.insecure_channel(self._address)  # type: ignore

        self._collector_stub = CollectorStub(self._channel)
        self._broker_stub = BrokerStub(self._channel)
        self._grpc_metadata = self.default_metadata()
        logger.debug("VDBHelper using metadata {}".format(self._grpc_metadata))
        self._ids: Dict[str, int] = None  # type: ignore
        self._vdb_metadata = None

    async def close(self) -> None:
        """Closes runtime gRPC channel."""
        if self._channel:
            await self._channel.close()

    def default_metadata(self):
        if os.environ.get("VEHICLEDATABROKER_DAPR_APP_ID"):
            return ("dapr-app-id", os.environ.get("VEHICLEDATABROKER_DAPR_APP_ID"))
        return None

    def __enter__(self) -> "VDBHelper":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        asyncio.run_coroutine_threadsafe(self.close(), asyncio.get_event_loop())

    async def __register_datapoints(self, datapoints: list):
        response = await self._collector_stub.RegisterDatapoints(
            RegisterDatapointsRequest(list=datapoints), metadata=self._grpc_metadata
        )
        return response

    async def __update_datapoints(self, datapoints: Mapping[int, Datapoint]):
        response = await self._collector_stub.UpdateDatapoints(
            UpdateDatapointsRequest(datapoints=datapoints), metadata=self._grpc_metadata
        )
        return response

    async def __get_datapoints(self, datapoints: list):
        response = await self._broker_stub.GetDatapoints(
            GetDatapointsRequest(datapoints=datapoints), metadata=self._grpc_metadata
        )
        return response

    async def get_vdb_metadata(self, names=[]):
        """Requests Metadata from VDB, allows for optional list of names

        Args:
            names (list, optional): List of names to get. Defaults to [].

        Returns:
            _type_: list, can be converted to json using parse_vdb_metadata()
        """
        response = await self._broker_stub.GetMetadata(
            GetMetadataRequest(names=names), metadata=self._grpc_metadata
        )
        return response.list

    def vdb_metadata_to_json(self, metadata) -> list:
        """Parses metadata.list to json format

        Args:
            metadata (_type_): MetaDataReply.list

        Returns:
            list: Json objects
        """
        return [
            {
                "id": m.id,
                "name": m.name,
                "data_type": m.data_type,
                "description": m.description,
            }
            for m in metadata
        ]

    def datapoint_to_dict(self, name: str, dp: Datapoint) -> dict:
        """Convert Datapoint object to dictionary

        Args:
            name (str): Datapoint Name
            dp (Datapoint): Datapoint

        Returns:
            dict: with keys "name", "ts", "value", "type"
        """
        value_type = dp.WhichOneof("value")
        if value_type:
            # try to get directly dp.${which_one} attribute
            value = getattr(dp, value_type)
        ts = (
            dp.timestamp.seconds + int(dp.timestamp.nanos / 10**6) / 1000
        )  # round to msec
        result = {"name": name, "ts": ts, "value": value, "type": value_type}
        return result

    async def get_vdb_datapoints(self, datapoints=None):
        if datapoints is None:
            await self.__initialize_metadata()
            datapoints = self._ids.keys()

        response = await self.__get_datapoints(datapoints=datapoints)
        # map<string, Datapoint> datapoints = 1;
        return response

    async def __initialize_metadata(self, names=[]) -> None:
        if self._ids is None:
            self._ids = {}
            response = await self._broker_stub.GetMetadata(
                [], metadata=self._grpc_metadata
            )
            self._vdb_metadata = response.list

            for item in response.list:
                self._ids[item.name] = item.id

    async def __get_or_create_datapoint_id_by_name(
        self, name: str, data_type: DataType
    ):
        await self.__initialize_metadata()

        key_list = self._ids.keys()
        if name not in key_list:
            response = await self.register_datapoint(name, data_type)
            datapoint_id = int(response)
            self._ids[name] = datapoint_id

        return self._ids[name]

    async def register_datapoint(self, name: str, data_type: DataType) -> int:
        await self.__initialize_metadata()

        registration_metadata = RegistrationMetadata()
        registration_metadata.name = name
        registration_metadata.data_type = data_type
        registration_metadata.description = ""
        registration_metadata.change_type = ChangeType.CONTINUOUS

        response = await self.__register_datapoints(datapoints=[registration_metadata])
        metadata_id = int(response.results[name])
        self._ids[name] = metadata_id
        return metadata_id

    async def set_int32_datapoint(self, name: str, value: int):
        datapoint = Datapoint()
        datapoint.int32_value = value
        datapoint_id = await self.__get_or_create_datapoint_id_by_name(
            name, DataType.INT32  # type: ignore
        )
        return await self.__update_datapoints({datapoint_id: datapoint})

    async def set_uint32_datapoint(self, name: str, value: int):
        datapoint = Datapoint()
        datapoint.uint32_value = value
        datapoint_id = await self.__get_or_create_datapoint_id_by_name(
            name, DataType.UINT32  # type: ignore
        )
        return await self.__update_datapoints({datapoint_id: datapoint})

    async def set_float_datapoint(self, name: str, value: float):
        datapoint = Datapoint()
        datapoint.float_value = value
        datapoint_id = await self.__get_or_create_datapoint_id_by_name(
            name, DataType.FLOAT  # type: ignore
        )
        return await self.__update_datapoints({datapoint_id: datapoint})

    def __get_grpc_error(self, err: grpc.RpcError) -> str:
        status_code = err.code()
        return "grpcError[Status:{} {}, details:'{}']".format(
            status_code.name, status_code.value, err.details()
        )

    async def subscribe_datapoints(
        self,
        query: str,
        sub_callback: Callable[[str, Datapoint], None],
        timeout: Optional[int] = None,
    ) -> None:
        try:
            request = SubscribeRequest(query=query)
            logger.info("broker.Subscribe('{}')".format(query))
            response = self._broker_stub.Subscribe(
                request, metadata=self._grpc_metadata, timeout=timeout
            )
            # NOTE: async before iteration is crucial here with aio.channel!
            async for subscribe_reply in response:
                logger.debug("Streaming SubscribeReply ...")
                """ from broker.proto:
                message SubscribeReply {
                    // Contains the fields specified by the query.
                    // If a requested data point value is not available, the corresponding
                    // Datapoint will have it's respective failure value set.
                    map<string, databroker.v1.Datapoint> fields = 1;
                }"""
                if not hasattr(subscribe_reply, "fields"):
                    raise Exception("Missing 'fields' in {}".format(subscribe_reply))

                logger.debug("SubscribeReply.{}".format(subscribe_reply.fields))
                for name in subscribe_reply.fields:
                    dp = subscribe_reply.fields[name]
                    try:
                        logger.debug("Calling sub_callback({}, dp:{})".format(name, dp))
                        sub_callback(name, dp)
                    except Exception:
                        logging.exception("sub_callback() error", exc_info=True)
                        pass
            logger.debug("Streaming SubscribeReply done...")

        except grpc.RpcError as e:
            if (
                e.code() == grpc.StatusCode.DEADLINE_EXCEEDED
            ):  # expected code if we used timeout, just stop subscription
                logger.debug("Exitting after timeout: {}".format(timeout))
            else:
                logging.error(
                    "broker.Subscribe({}) failed!\n --> {}".format(
                        query, self.__get_grpc_error(e)
                    )
                )
                raise e
        except Exception:
            logging.exception("broker.Subscribe() error", exc_info=True)


def __on_subscribe_event(name: str, dp: Datapoint) -> None:
    value_type = dp.WhichOneof("value")
    if value_type:
        # try to get directly dp.${which_one} attribute
        value = getattr(dp, value_type)
    ts = (
        dp.timestamp.seconds + int(dp.timestamp.nanos / 10**6) / 1000
    )  # round to msec

    print(
        "#SUB# name:{}, value:{}, value_type:{}, ts:{}".format(
            name, value, value_type, ts
        ),
        flush=True,
    )


async def main() -> None:
    LOG_LEVEL = os.environ.get("LOG_LEVEL", "INFO")
    logging.basicConfig(format="<%(levelname)s>\t%(message)s", level=LOG_LEVEL)

    vdb_addr = os.environ.get("VDB_ADDR", "localhost:55555")
    query = os.environ.get("QUERY", "SELECT Vehicle.Cabin.Seat.Row1.Pos1.Position")
    helper = VDBHelper(vdb_addr)

    await helper.subscribe_datapoints(
        query, sub_callback=__on_subscribe_event, timeout=1
    )
    await helper.close()


if __name__ == "__main__":
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
