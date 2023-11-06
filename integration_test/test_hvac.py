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
import subprocess  # nosec B404
import time
from threading import Thread

import pytest
from sdv.databroker.v1.types_pb2 import Datapoint
from vdb_helper import SubscribeRunner, VDBHelper

# prevents dumping "E0617: Fork support is only compatible with the epoll1 and poll polling strategies"
os.environ["GRPC_ENABLE_FORK_SUPPORT"] = "false"

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "DEBUG"))


# Env USE_DAPR forces usage of vscode tasks and scripts using 'dapr run' with predefined ports
USE_DAPR = os.getenv("USE_DAPR", "1") != "0"

if USE_DAPR:
    DEFAULT_VDB_ADDRESS = "localhost:55555"
    DEFAULT_SCRIPT_HVAC_CLI = "./task-hvac-cli.sh"
else:
    # talk to exposed port (host) from databroker container
    DEFAULT_VDB_ADDRESS = "localhost:35555"
    DEFAULT_SCRIPT_HVAC_CLI = "./it-hvac-cli.sh"

VDB_ADDRESS = os.environ.get("VDB_ADDRESS", DEFAULT_VDB_ADDRESS)


SCRIPT_HVAC_CLI = os.getenv(
    "SCRIPT_HVAC_CLI", os.path.join(os.path.dirname(__file__), DEFAULT_SCRIPT_HVAC_CLI)
)


def execute_script_thread(script_args: list, quiet: bool = False) -> Thread:
    client_thread = Thread(
        target=execute_script,
        args=(script_args, True),
        daemon=True,
        name="execute_script({})".format(script_args),
    )
    return client_thread


def execute_script(args: list, quiet: bool = False) -> None:
    logger.info("$ {}".format(" ".join(args)))

    fork_env = os.environ.copy()
    if quiet:  # reduce python logging
        fork_env["CLI_LOG_LEVEL"] = "WARNING"
        fork_stdout = subprocess.DEVNULL
    else:
        fork_stdout = subprocess.PIPE
    try:
        process = subprocess.run(  # nosec B603
            args,
            check=True,
            timeout=30,
            stdout=fork_stdout,
            # stderr=subprocess.DEVNULL,
            env=fork_env,
            # capture_output=not quiet
        )
        # , shell=True, capture_output=True, check=True
        logger.debug("rc:{}".format(process.returncode))
        # logger.debug("-->>\n[out] {}\n\[err]{}\n".format(process.stdout, process.stderr))
    except Exception as ex:
        logging.error(ex)
        raise ex


@pytest.fixture
async def setup_helper() -> VDBHelper:
    logger.info("Using VDB_ADDR={}".format(VDB_ADDRESS))
    helper = VDBHelper(VDB_ADDRESS)
    logger.setLevel(os.getenv("LOG_LEVEL", "DEBUG"))
    return helper


# @pytest.mark.asyncio
# async def test_hvac_vdb_connection() -> None:
#     logger.info("Connecting to VehicleDataBroker {}".format(VDB_ADDRESS))
#     helper = VDBHelper(VDB_ADDRESS)
#     logger.info("VDBHelper._address =  {}".format(helper._address))
#     await helper.close()


@pytest.mark.asyncio
async def test_hvac_metadata_registered(setup_helper: VDBHelper) -> None:
    helper = setup_helper
    feeder_names = [
        "Vehicle.Cabin.DesiredAmbientAirTemperature",
        "Vehicle.Cabin.IsAirConditioningActive",
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
        logger.info("[hvac] Found metadata: {}".format(name_reg))
        # TODO: check for expected types?
        # assert (  # nosec B101
        #     name_reg["data_type"] == DataType.UINT32
        # ), "{} datatype is {}".format(name, name_reg["data_type"])

    await helper.close()


@pytest.mark.asyncio
async def test_hvac_events(setup_helper: VDBHelper) -> None:
    helper: VDBHelper = setup_helper

    timeout = 5
    name1 = "Vehicle.Cabin.DesiredAmbientAirTemperature"
    name2 = "Vehicle.Cabin.IsAirConditioningActive"
    alias1 = "temp"
    alias2 = "active"

    query = "SELECT {} as {}, {} as {}".format(name1, alias1, name2, alias2)

    # hvac_cli = HVACTestClient()
    # hvac_cli.execute_methods(AcStatus.ON, 23.0)

    client_cmd = [SCRIPT_HVAC_CLI, "42.0", "ON"]
    execute_script(client_cmd)

    def client_thread_runner() -> None:
        try:
            time.sleep(0.2)
            for temp in range(20, 23):
                state = "ON" if (temp % 3) == 0 else "OFF"
                logger.info(">>> Hvac CLI: %s %f", state, temp)
                execute_script([SCRIPT_HVAC_CLI, "{}".format(temp), state], True)
            # execute_script([ SCRIPT_HVAC_CLI, "42.0", "OFF" ])
            # time.sleep(1.2)
            # execute_script([ SCRIPT_HVAC_CLI + "!", "-12", "OFF" ])
        except Exception:
            logger.exception("Test thread failed!", exc_info=True)

    events = []
    # inner function for collecting subscription events

    def inner_callback(name: str, dp: Datapoint):
        dd = helper.datapoint_to_dict(name, dp)
        events.append(dd)

    # start client requests in different thread as subscribe_datapoints will block...
    client_thread = Thread(
        target=client_thread_runner, daemon=False, name="test_hvac_events"
    )
    client_thread.start()

    logger.info("# subscribing('{}', timeout={})".format(query, timeout))
    await helper.subscribe_datapoints(
        query, timeout=timeout, sub_callback=inner_callback
    )
    logger.info("Received events:{}".format(events))
    client_thread.join()

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


# async def _subscibe_and_wait_events(helper: VDBHelper, query: str, events: list, timeout=3) -> list:

#     events.clear()

#     def inner_callback(name: str, dp: Datapoint):
#         """ inner function for collecting subscription events """
#         dd = helper.datapoint_to_dict(name, dp)
#         if not name in events:
#             events[name] = []
#         events[name].append(dd)

#     # start client requests in different thread as subscribe_datapoints will block...
#     logger.info("# subscribing('{}', timeout={})".format(query, timeout))
#     await helper.subscribe_datapoints(
#         query, timeout=timeout, sub_callback=inner_callback
#     )
#     return events

# def _find_event_value(name: str, value: any, events: Dict[str, List[Datapoint]]) -> Datapoint:
#     if not name in events:
#         return None
#     for dp in events[name]:
#         val = dp["value"]
#         if val == value:
#             return dp
#         elif isinstance(val, float) and value == pytest.approx(val, 0.1):
#             return dp
#     return None


async def _test_hvac_event_changes(setup_helper: VDBHelper) -> None:
    helper: VDBHelper = setup_helper

    name_temp = "Vehicle.Cabin.DesiredAmbientAirTemperature"
    name_status = "Vehicle.Cabin.IsAirConditioningActive"
    query = "SELECT {}, {}".format(name_temp, name_status)
    timeout = 5

    # iterate over this list and expect to have received each of the tuples for temp, status
    expected_values = [(42.0, True), (-273.15, False), (10000000.0, True)]

    # ## FIXME: can't find proper way to use it from ../hvac_service/
    # hvac_cli = HVACTestClient()
    # hvac_cli.execute_methods(AcStatus.ON, 23.0)

    for temp, status in expected_values:

        ac_temp = str(temp)
        ac_status = "ON" if status else "OFF"

        sub = SubscribeRunner(VDB_ADDRESS, query, timeout)
        sub.start()

        execute_script_thread([SCRIPT_HVAC_CLI, "-1e-8", "OFF"], quiet=True).start()
        execute_script_thread([SCRIPT_HVAC_CLI, ac_temp, ac_status], quiet=True).start()
        # execute_script_thread([SCRIPT_HVAC_CLI, "42.00001", "OFF"], quiet=True).start()

        events = sub.get_events()
        logger.info("--> Received events:{}".format(events))

        assert (  # nosec B101
            len(events) > 0
        ), "No events from hvac service for {} sec.".format(timeout)
        # check for expected event names
        assert (  # nosec B101
            name_temp in events and name_status in events
        ), "Expected events not found in: {}".format(events)

        event_values1 = sub.get_dp_values(name_temp)
        event_values2 = sub.get_dp_values(name_status)

        logger.info("  '%s' values = %s", name_temp, event_values1)
        logger.info("  '%s' values = %s", name_status, event_values2)

        dp_temp = sub.find_dp_value(name_temp, temp)
        dp_status = sub.find_dp_value(name_status, status)
        assert dp_temp, "Expected temp {} not found in: {}".format(  # nosec B101
            temp, event_values1
        )
        assert dp_status, "Expected status {} not found in: {}".format(  # nosec B101
            temp, event_values2
        )

    await helper.close()


if __name__ == "__main__":
    # execute_script([SCRIPT_HVAC_CLI, "1000", "ON"])
    pytest.main(["-vvs", "--log-cli-level=INFO", os.path.abspath(__file__)])
