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

import json
import logging
import os
import subprocess  # nosec

import grpc
import pytest
from gen_proto.sdv.databroker.v1.types_pb2 import Datapoint, DataType
from vdb_helper import VDBHelper

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "INFO"))

# Env USE_DAPR forces usage of vscode tasks and scripts using 'dapr run' with predefined ports
USE_DAPR = os.getenv("USE_DAPR", "1") != "0"

if USE_DAPR:
    DEFAULT_VDB_ADDRESS = "localhost:55555"
    DEFAULT_SCRIPT_SEAT_MOVE = "task-seat-move.sh"
else:
    DEFAULT_VDB_ADDRESS = "localhost:35555"
    DEFAULT_SCRIPT_SEAT_MOVE = "it-seat-move.sh"

VDB_ADDRESS = os.environ.get("VDB_ADDRESS", DEFAULT_VDB_ADDRESS)

SCRIPT_SEAT_MOVE = os.getenv(
    "SCRIPT_SEAT_MOVE",
    os.path.join(os.path.dirname(__file__), DEFAULT_SCRIPT_SEAT_MOVE),
)


def execute_script(args: list) -> None:
    logger.info("$ {}".format(" ".join(args)))
    try:
        process = subprocess.run(args, check=True)  # nosec
        # , shell=True, capture_output=True, check=True)
        logger.debug("rc:{}".format(process.returncode))
        # logger.debug("-->>\n[out] {}\n\[err]{}\n".format(process.stdout, process.stderr))
    except Exception as ex:
        logging.exception(ex)


@pytest.fixture
async def setup_helper() -> VDBHelper:
    logger.info("Using VDB_ADDR={}".format(VDB_ADDRESS))
    helper = VDBHelper(VDB_ADDRESS)
    return helper


@pytest.mark.asyncio
async def test_vdb_metadata_get(setup_helper: VDBHelper) -> None:
    helper = setup_helper
    name = os.getenv("TEST_NAME", "Vehicle.Cabin.Seat.Row1.Pos1.Position")

    meta = await helper.get_vdb_metadata()
    # logger.debug("# get_vdb_metadata() -> \n{}".format(str(meta).replace('\n', ' ')))

    assert len(meta) > 0, "VDB Metadata is empty"  # nosec B101
    meta_list = helper.vdb_metadata_to_json(meta)
    logger.debug("get_vdb_metadata()->\n{}".format(json.dumps(meta_list, indent=2)))

    meta_names = [d["name"] for d in meta_list]

    assert name in meta_names, "{} not registered!".format(name)  # nosec B101
    name_reg = meta_list[meta_names.index(name)]

    assert len(name_reg) == 4 and name_reg["name"] == name  # nosec B101
    logger.info("Found metadata: {}".format(name_reg))

    assert (  # nosec B101
        name_reg["data_type"] == DataType.UINT32
    ), "{} datatype is {}".format(name, name_reg["data_type"])
    await helper.close()


@pytest.mark.asyncio
async def test_subscribe_seat_pos_0(setup_helper: VDBHelper) -> None:
    helper: VDBHelper = setup_helper
    name = os.getenv("TEST_NAME", "Vehicle.Cabin.Seat.Row1.Pos1.Position")
    query = "SELECT {}".format(name)

    start_value = int(os.getenv("TEST_START_VALUE", "500"))
    expected_value = int(os.getenv("TEST_VALUE", "0"))
    timeout = int(os.getenv("TEST_TIMEOUT", "10"))

    # initiate seat move to 42
    logger.info(" -- moving seat to initial pos: {} (sync)".format(start_value))

    # sync move to predefined pos
    execute_script([SCRIPT_SEAT_MOVE, str(start_value), "-w"])

    events = []
    # inner function for collecting subscription events

    def inner_callback(name: str, dp: Datapoint):
        dd = helper.datapoint_to_dict(name, dp)
        events.append(dd)

    logger.info(" -- moving seat to test position {} (async)...".format(expected_value))
    execute_script([SCRIPT_SEAT_MOVE, str(expected_value)])

    logger.debug(
        "\n# subscribing('{}', timeout={}), expecting:{}".format(
            query, timeout, expected_value
        )
    )
    await helper.subscribe_datapoints(
        query, timeout=timeout, sub_callback=inner_callback
    )

    assert (  # nosec B101
        len(events) > 0
    ), "Not received events for '{}' in {} sec.".format(name, timeout)
    # list of received names
    event_names = set([e["name"] for e in events])
    # list of received values
    event_values_name = [e["value"] for e in events if e["name"] == name]

    logger.debug("  --> names  : {}".format(event_names))
    # event_values = [e['value'] for e in events]
    # logger.debug("  --> values : {}".format(event_values))
    logger.debug("  --> <{}> : {}".format(name, event_values_name))

    assert name in event_names, "{} event not received! {}".format(  # nosec B101
        name, event_names
    )

    assert (  # nosec B101
        expected_value in event_values_name
    ), "{} value {} missing! {}".format(name, expected_value, event_values_name)

    await helper.close()


@pytest.mark.asyncio
async def test_subscribe_seat_pos_where_eq(setup_helper: VDBHelper) -> None:
    helper = setup_helper

    name = os.getenv("TEST_NAME", "Vehicle.Cabin.Seat.Row1.Pos1.Position")
    expected_value = int(os.getenv("TEST_VALUE", "1000"))
    timeout = int(os.getenv("TEST_TIMEOUT", "10"))

    query = "SELECT {} where {} = {}".format(name, name, expected_value)

    events = []
    # inner function for collecting subscription events

    def inner_callback(name: str, dp: Datapoint):
        dd = helper.datapoint_to_dict(name, dp)
        events.append(dd)

    logger.info(" -- moving seat to test position {} (async)...".format(expected_value))
    execute_script([SCRIPT_SEAT_MOVE, str(expected_value)])

    logger.debug(
        "\n# subscribing('{}', timeout={}), expecting:{}".format(
            query, timeout, expected_value
        )
    )
    await helper.subscribe_datapoints(
        query, timeout=timeout, sub_callback=inner_callback
    )

    assert (  # nosec B101
        len(events) > 0
    ), "Not received subscription events for '{}' in {} sec.".format(name, timeout)

    # list of received names
    event_names = set([e["name"] for e in events])
    # list of received values
    event_values_name = [e["value"] for e in events if e["name"] == name]

    logger.debug("  --> names  : {}".format(event_names))
    # event_values = [e['value'] for e in events]
    # logger.debug("  --> values : {}".format(event_values))
    logger.debug("  --> <{}> : {}".format(name, event_values_name))

    assert name in event_names, "{} event not received! {}".format(  # nosec B101
        name, event_names
    )

    assert (  # nosec B101
        expected_value in event_values_name
    ), "{} value {} missing! {}".format(name, expected_value, event_values_name)

    assert (  # nosec B101
        len(set(event_values_name)) == 1
    ), "Should get only 1 value for {}, got: {}".format(name, event_values_name)
    await helper.close()


@pytest.mark.asyncio
async def test_subscribe_seat_pos_where_error(setup_helper: VDBHelper) -> None:
    helper = setup_helper

    name = os.getenv("TEST_NAME", "Vehicle.Cabin.Seat.Row1.Pos1.Position")
    expected_value = int(os.getenv("TEST_VALUE", "-42"))
    timeout = int(os.getenv("TEST_TIMEOUT", "10"))
    query = "SELECT {} where <invalid>".format(name)

    events = []

    # inner function for collecting subscription events

    def inner_callback(name: str, dp: Datapoint):
        dd = helper.datapoint_to_dict(name, dp)
        events.append(dd)

    with pytest.raises(grpc.RpcError):
        logger.debug(
            "\n# subscribing('{}', timeout={}), expecting:{}".format(
                query, timeout, expected_value
            )
        )
        await helper.subscribe_datapoints(
            query, timeout=timeout, sub_callback=inner_callback
        )

    assert (  # nosec B101
        len(events) == 0
    ), "Should not receive events for query:'{}'. Got {}".format(query, events)

    await helper.close()


async def main() -> None:
    log_level = os.environ.get("LOG_LEVEL", "INFO")
    logging.basicConfig(format="<%(levelname)s>\t%(message)s", level=log_level)


if __name__ == "__main__":
    # execute_script([SCRIPT_SEAT_MOVE, "500", "-w"])
    pytest.main(["-vvs", "--log-cli-level=INFO", os.path.abspath(__file__)])
