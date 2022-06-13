#!/usr/bin/env python3
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
import getopt
import json
import logging
import os
import signal
import sys
import time

import grpc
from gen_proto.sdv.databroker.v1 import broker_pb2
from gen_proto.sdv.databroker.v1.broker_pb2_grpc import BrokerStub
from gen_proto.sdv.databroker.v1.types_pb2 import DataType

SEAT_POS = "Vehicle.Cabin.Seat.Row1.Pos1.Position"

# allow log level change via 'LOG_LEVEL' env. var
LOG_LEVEL = os.environ.get("LOG_LEVEL", "INFO")
logging.basicConfig(format="<%(levelname)s>\t%(message)s", level=LOG_LEVEL)
logger = logging.getLogger(__name__)

__HEADER_PRINTED = False


def on_change_event(key, value, value_type, timestamp):
    global __HEADER_PRINTED
    if not __HEADER_PRINTED:
        print(
            "##### | {:40s} | {:8s} | {:20s} | {:16} |".format(
                "[Name]", "[Value]", "[ValueType]", "[Timestamp]"
            ),
            flush=True,
        )
        __HEADER_PRINTED = True
    print(
        "#SUB# | {:40s} | {:8s} | {:20s} | {:<16.3f} |".format(
            str(key), str(value), value_type, timestamp
        ),
        flush=True,
    )


def on_change_event_json(key, value, value_type, timestamp):
    sub_event = {
        "name": str(key),
        "value": value,
        "valueType": value_type,
        "ts": timestamp,
    }
    event_json = json.dumps(sub_event)  # , sort_keys=True)
    print("#SUB-JSON# {}".format(event_json), flush=True)


def print_json_metadata(metadata) -> str:
    meta_list = []
    for meta in metadata:
        meta_list.append(
            {
                "id": meta.id,
                "name": meta.name,
                "dataType": DataType.Name(meta.data_type),
                "desc": meta.description,
            }
        )

    meta_json = json.dumps(meta_list, sort_keys=True)
    print("#META-JSON# {}".format(meta_json), flush=True)
    return meta_json


class BrokerSubscribe(object):

    VERBOSE = False

    def __init__(
        self, databroker_address="localhost:55555", max_events=0, timeout=0
    ) -> None:
        # logger.setLevel(logging.DEBUG)

        self.configDir = os.path.dirname(os.path.realpath(__file__))
        self.databroker_address = databroker_address
        self.max_events = max_events
        self.timeout = timeout
        self._running = False
        self._events = 0
        self._ts = None

        # GRPC: Connect to the collector service
        logger.info("Connecting to databroker: {}".format(self.databroker_address))
        self._channel = grpc.insecure_channel(self.databroker_address)
        self.broker_stub = BrokerStub(self._channel)

    def _get_grpc_error(self, err):
        status_code = err.code()
        return "\n  GrpcError[Status:{} {}]\n  GRPC Details:'{}']".format(
            status_code.name, status_code.value, err.details()
        )

    async def close(self):
        """Close runtime gRPC channel."""
        if self._channel:
            await self._channel.close()

    def __enter__(self) -> "BrokerSubscribe":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        asyncio.run_coroutine_threadsafe(self.close(), asyncio.get_event_loop())

    def _parse_datapoint(self, dp):
        """
        *
        * Parse protobuf definitions.
        *
        * @returns: ( <value>, <'oneof value'> )

        message Datapoint {
                // Timestamp of the value
                google.protobuf.Timestamp timestamp = 1;

                // values
                oneof value {
                        Failure failure_value    = 10; // from enum Failure
                        string string_value      = 11;
                        bool bool_value          = 12;
                        sint32 int32_value       = 13;
                        sint64 int64_value       = 14;
                        uint32 uint32_value      = 15;
                        uint64 uint64_value      = 16;
                        float float_value        = 17;
                        double double_value      = 18;
                        StringArray string_array = 21;
                        BoolArray bool_array     = 22;
                        Int32Array int32_array   = 23;
                        Int64Array int64_array   = 24;
                        Uint32Array uint32_array = 25;
                        Uint64Array uint64_array = 26;
                        FloatArray float_array   = 27;
                        DoubleArray double_array = 28;
        }

        message StringArray {
                repeated string values = 1;
        }

        enum Failure {
                // The data point is known, but doesn't have a valid value
                INVALID_VALUE = 0;
                // The data point is known, but no value is available
                NOT_AVAILABLE = 1;
                // Unknown datapoint
                UNKNOWN_DATAPOINT = 2;
                // Access denied
                ACCESS_DENIED = 3;
                // Unexpected internal error
                INTERNAL_ERROR = 4;
        }
        """

        one_value = dp.WhichOneof("value")
        logger.debug("  dp.value: {}".format(one_value))
        # _inspect_object("  --> one_value", one_value, public_only=True, extended=True)
        if dp.HasField("timestamp"):
            ts = (
                dp.timestamp.seconds + int(dp.timestamp.nanos / 10**6) / 1000
            )  # round to msec

        logger.debug("  dp.timestamp: {}".format(ts))

        value = None
        if one_value is None:
            raise Exception('"oneof value" is missing in: {}'.format(dp))
        elif one_value == "failure_value":
            value = dp.failure_value
        elif one_value == "string_value":
            value = dp.string_value
        elif one_value == "bool_value":
            value = dp.bool_value
        elif one_value == "int32_value":
            value = dp.int32_value
        elif one_value == "int64_value":
            value = dp.int64_value
        elif one_value == "uint32_value":
            value = dp.uint32_value
        elif one_value == "uint64_value":
            value = dp.uint64_value
        elif one_value == "float_value":
            value = dp.float_value
        elif one_value == "double_value":
            value = dp.double_value
        elif one_value == "string_array":
            value = dp.string_array.values
        elif one_value == "bool_array":
            value = dp.bool_array.values
        elif one_value == "int32_array":
            value = dp.int32_array.values
        elif one_value == "int64_array":
            value = dp.int64_array.values
        elif one_value == "uint32_array":
            value = dp.uint32_array.values
        elif one_value == "uint64_array":
            value = dp.uint64_array.values
        elif one_value == "float_array":
            value = dp.float_array.values
        elif one_value == "double_array":
            value = dp.double_array.values
        else:
            raise Exception("Unknown value {} in Datapoint:{}".format(one_value, dp))

        result = {}
        result["value"] = value
        result["ts"] = ts
        result["type"] = one_value

        logger.debug("_parse_datapoint() -> {}".format(result))
        return result

    def get_metadata(self, names=[]):
        """
        Wraps broker.GetMetadata(names) and returns list of MetaData
        @param names: list of names or [] for returning full MetaData
        @return List of registered MetaData entries
        """
        try:
            request = broker_pb2.GetMetadataRequest()
            for n in names:
                request.names.append(n)
            response = self.broker_stub.GetMetadata(request)
            assert response.list is not None  # nosec
            logger.debug("broker.GetMetadata({}) -> {}".format(names, response.list))

            # message GetMetadataReply {
            #     // Contains metadata of the requested data points. If a data point
            #     // doesn't exist (i.e. not known to the Data Broker) the corresponding
            #     // Metadata isn't part of the returned list.
            #     repeated databroker.v1.Metadata list = 1;
            # }
            # message Metadata {
            #   // Id to be used in "get" and "subscribe" requests. Ids stay valid during
            #   // one power cycle, only.
            #   int32 id               = 1;
            #   string name            = 4;
            #   DataType data_type     = 5;
            #   ChangeType change_type = 6;  // CONTINUOUS or STATIC or ON_CHANGE
            #   string description     = 7;
            # }
            for metadata in response.list:
                logger.debug(
                    "  * Metadata[id:{}, name:{}, type:{}, descr:{}]".format(
                        metadata.id,
                        metadata.name,
                        metadata.data_type,
                        metadata.description,
                    )
                )

            return response.list
        except grpc.RpcError as e:
            logging.error(
                "broker.GetMetadata({}) failed! {}".format(
                    names, self._get_grpc_error(e)
                )
            )
            raise e

    def print_meta_data(self, meta):
        print(
            "       | {:2} | {:40s} | {:12s} | {}".format(
                "ID", "[Name]", "[DataType]", "[Description]"
            ),
            flush=True,
        )
        for metadata in meta:
            print(
                "#META# | {:2} | {:40s} | {:12s} | {}".format(
                    metadata.id,
                    metadata.name,
                    DataType.Name(metadata.data_type),
                    metadata.description,
                ),
                flush=True,
            )
            print_json_metadata(meta)
            # print('#META# {{ "id":{}, "name":"{}", "data_type":"{}", "descr":"{}" }}'.format(
            #    metadata.id, metadata.name, DataType.Name(metadata.data_type), metadata.description), flush=True)
            # print("#META# {}".format(str(x).replace("\n", " ")), flush=True)

    def get_wildcard_query(self):
        """Gets All Metadata entries from Broker and generates
        SELECT query including all of available names

        Returns:
                        str: Query (for Broker.Subscribe()) including all available Metadata names
        """
        meta = self.get_metadata([])
        self.print_meta_data(meta)

        # for x in meta:
        # 	print("#META# {}".format(str(x).replace("\n", " ")))
        # 	id = x.id if hasattr(x, 'id') else None
        # 	print("#META# { id:{}, name:{}, type:{}, desc:{} }".format(id ,
        #       x.name, x.data_type, x.description), flush=True)

        all = ",\n  ".join([(x.name) for x in meta])
        query = "SELECT {}".format(all)
        return query

    def get_registered_metadata(self, names=[]):
        meta = self.get_metadata(names)
        self.print_meta_data(meta)
        return meta

    async def subscribe_datapoints(self, query, sub_callback=None):
        try:
            request = broker_pb2.SubscribeRequest()
            request.query = query
            logger.info("broker.Subscribe('{}')".format(query))
            if self.timeout > 0:
                response = self.broker_stub.Subscribe(request, timeout=self.timeout)
            else:
                response = self.broker_stub.Subscribe(request)

            for subscribe_reply in response:
                """
                message SubscribeReply {
                        // Contains the fields specified by the query.
                        // If a requested data point value is not available, the corresponding
                        // Datapoint will have it's respective failure value set.
                        map<string, databroker.v1.Datapoint> fields = 1;
                }"""

                # SubscribeReply.fields:
                #   map<string, databroker.v1.Datapoint>
                if not hasattr(subscribe_reply, "fields"):
                    logger.warning("Missing 'fields' in {}".format(subscribe_reply))
                    continue

                logger.debug("SubscribeReply.{}".format(subscribe_reply))
                map = subscribe_reply.fields
                for key in map:
                    dp = map[key]
                    parsed = self._parse_datapoint(dp)
                    logger.info(
                        "EVENT: {}={} ({}) TS:{}".format(
                            key, parsed["value"], parsed["type"], parsed["ts"]
                        )
                    )
                    if sub_callback:
                        logger.debug("calling cb:{}".format(sub_callback))
                        try:
                            sub_callback(
                                key, parsed["value"], parsed["type"], parsed["ts"]
                            )
                        except Exception:
                            logging.exception("sub_callback() error", exc_info=True)
                            pass
                # apply limits per reply, not for each field in reply...
                if self.timeout > 0:
                    now = time.time()
                    if self._ts and now - self._ts >= self.timeout:
                        logger.info("Terminating after {} events.".format(self._events))
                        self._running = False
                        return
                    self._ts = now
                if self.max_events > 0:
                    self._events += 1
                    if self._events >= self.max_events:
                        logger.info("Terminating after {} events.".format(self._events))
                        self._running = False
                        return

        except grpc.RpcError as e:
            if e.code() == grpc.StatusCode.DEADLINE_EXCEEDED:
                # expected code if we used timeout, just stop subscription
                logger.info("Exitting due to idle timeout: {}".format(self.timeout))
                self._running = False
            else:
                logging.error(
                    "broker.Subscribe() failed! {}".format(self._get_grpc_error(e))
                )
                raise e
        except Exception:
            logging.exception("broker.Subscribe() error", exc_info=True)


def main(argv):
    """Main function"""

    default_addr = "localhost:55555"
    default_query = "SELECT {}".format(SEAT_POS)

    _usage = (
        "Usage: ./broker_subscriber.py --addr <host:name>"  # shorten line
        " [ --get-meta=META | --query <QUERY> --timeout <TIMEOUT> --count <COUNT> ]\n\n"
        "Environment:\n"
        "  'BROKER_ADDR' Default: {}\n"
        "  'QUERY'       SQL datapoint query. ('*' = subscribe for all meta). Default: {}\n"
        "  'COUNT'       Receive specified count of events and exit (0=inf)\n"
        "  'TIMEOUT'     Abort receiving if no data comes for specified timeout in seconds (0=inf)\n"
        "  'META'        Comma separated list of datapoint names to query. ('*' = all meta)\n".format(
            default_addr, default_query
        )
    )

    # environment values (overridden by cmdargs)
    broker_addr = os.environ.get("BROKER_ADDR", default_addr)
    query = os.environ.get("QUERY", default_query)
    count = int(os.environ.get("COUNT", "0"))
    timeout = float(os.environ.get("TIMEOUT", "0"))
    meta = os.environ.get("META")

    # parse cmdline args
    try:
        opts, args = getopt.getopt(
            argv, "ha:q:c:t:g", ["addr=", "query=", "count=", "timeout=", "get-meta="]
        )
        for opt, arg in opts:
            if opt == "-h":
                print(_usage)
                sys.exit(0)
            elif opt in ("-a", "--addr"):
                broker_addr = arg
            elif opt in ("-q", "--query"):
                query = arg
            elif opt in ("-c", "--count"):
                count = int(arg)
            elif opt in ("-t", "--timeout"):
                timeout = float(arg)
            elif opt in ("-g", "--get-meta"):
                if arg is not None:
                    meta = opt
                else:
                    meta = "*"
            else:
                print("Unhandled arg: {}".format(opt))
                print(_usage)
                sys.exit(1)
    except getopt.GetoptError:
        print(_usage)
        sys.exit(1)

    listener = BrokerSubscribe(broker_addr, max_events=count, timeout=timeout)
    if query == "*":
        query = listener.get_wildcard_query()
        # logger.info("Replaced '*' query with:\n{}", query);

    get_meta = None
    # parse meta arg/env var and split it to list
    if meta:
        if meta == "*":
            get_meta = []
        else:
            get_meta = meta.split(",")

    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    if get_meta is not None:
        meta = listener.get_registered_metadata(get_meta)
        print_json_metadata(meta)
    else:
        LOOP.run_until_complete(
            listener.subscribe_datapoints(query, on_change_event_json)
        )
    # LOOP.call_at(time.time() + 500, print("#### CALL_AT called!"))
    LOOP.close()


if __name__ == "__main__":
    main(sys.argv[1:])
