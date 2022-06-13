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

import pytest
from gen_proto.sdv.databroker.v1.types_pb2 import Datapoint
from vdb_helper import VDBHelper

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "DEBUG"))

# Env USE_DAPR forces usage of vscode tasks and scripts using 'dapr run' with predefined ports
USE_DAPR = os.getenv("USE_DAPR", "1") != "0"

if USE_DAPR:
    DEFAULT_VDB_ADDRESS = "localhost:55555"
else:
    DEFAULT_VDB_ADDRESS = "localhost:35555"

VDB_ADDRESS = os.environ.get("VDB_ADDRESS", DEFAULT_VDB_ADDRESS)


@pytest.fixture
async def setup_helper() -> VDBHelper:
    logger.info("Using VDB_ADDR={}".format(VDB_ADDRESS))
    helper = VDBHelper(VDB_ADDRESS)
    return helper


@pytest.mark.asyncio
async def test_feeder_vdb_connection() -> None:
    logger.info("Connecting to VehicleDataBrocker {}".format(VDB_ADDRESS))
    helper = VDBHelper(VDB_ADDRESS)
    logger.info("VDBHelper._address =  {}".format(helper._address))
    await helper.close()


@pytest.mark.asyncio
async def test_feeder_metadata_registered(setup_helper: VDBHelper) -> None:
    helper = setup_helper
    feeder_names = [
        "Vehicle.OBD.Speed",
        "Vehicle.Powertrain.Transmission.Gear",
        "Vehicle.Chassis.ParkingBrake.IsEngaged",
        "Vehicle.OBD.EngineLoad",
    ]

    meta = await helper.get_vdb_metadata(feeder_names)
    logger.debug(
        "# get_vdb_metadata({}) -> \n{}".format(
            feeder_names, str(meta).replace("\n", " ")
        )
    )

    assert len(meta) > 0, "VDB Metadata is empty"  # nosec B101
    assert len(meta) == len(  # nosec B101
        feeder_names
    ), "Filtered meta with unexpected size: {}".format(meta)
    meta_list = helper.vdb_metadata_to_json(meta)
    logger.debug("get_vdb_metadata() --> \n{}".format(json.dumps(meta_list, indent=2)))

    meta_names = [d["name"] for d in meta_list]

    for name in feeder_names:
        assert name in meta_names, "{} not registered!".format(name)  # nosec B101

        name_reg = meta_list[meta_names.index(name)]

        assert len(name_reg) == 4 and name_reg["name"] == name  # nosec B101
        logger.info("[feeder] Found metadata: {}".format(name_reg))
        # TODO: check for expected types?
        # assert (  # nosec B101
        #     name_reg["data_type"] == DataType.UINT32
        # ), "{} datatype is {}".format(name, name_reg["data_type"])

    await helper.close()


@pytest.mark.asyncio
async def test_feeder_events(setup_helper: VDBHelper) -> None:
    helper: VDBHelper = setup_helper

    timeout = 3
    name1 = "Vehicle.OBD.Speed"
    name2 = "Vehicle.OBD.EngineLoad"
    alias1 = "speed"
    alias2 = "load"

    query = "SELECT {} as {}, {} as {}".format(name1, alias1, name2, alias2)

    events = []
    # inner function for collecting subscription events

    def inner_callback(name: str, dp: Datapoint):
        dd = helper.datapoint_to_dict(name, dp)
        events.append(dd)

    logger.info("# subscribing('{}', timeout={})".format(query, timeout))

    await helper.subscribe_datapoints(
        query, timeout=timeout, sub_callback=inner_callback
    )
    logger.debug("Received events:{}".format(events))

    assert len(events) > 0, "No events from feeder for {} sec.".format(  # nosec B101
        timeout
    )

    # list of received names
    event_names = set([e["name"] for e in events])
    # list of received values
    alias_values1 = set([e["value"] for e in events if e["name"] == alias1])
    alias_values2 = set([e["value"] for e in events if e["name"] == alias2])

    logger.debug("  --> names  : {}".format(event_names))
    # event_values = [e['value'] for e in events]
    # logger.debug("  --> values : {}".format(event_values))
    # logger.debug("  --> <{}> : {}".format(name, event_values_name))

    assert set([alias1, alias2]) == set(  # nosec B101
        event_names
    ), "Unexpected event aliases received: {}".format(event_names)

    assert (  # nosec B101
        len(alias_values1) > 1
    ), "{} values not changing: {}. Is feeder running?".format(alias1, alias_values1)

    assert (  # nosec B101
        len(alias_values2) > 1
    ), "{} values not changing: {}. Is feeder running?".format(alias2, alias_values2)

    await helper.close()


if __name__ == "__main__":
    pytest.main(["-vvs", "--log-cli-level=INFO", os.path.abspath(__file__)])
