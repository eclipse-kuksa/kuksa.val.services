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

import logging
from abc import ABC, abstractmethod
from typing import Dict, List, NamedTuple, Tuple

from lib.action import ActionContext
from lib.behavior import Behavior
from lib.datapoint import MockedDataPoint
from lib.dsl import _mocked_datapoints, _required_datapoint_paths
from lib.trigger import (
    ClockTrigger,
    ClockTriggerResult,
    EventTrigger,
    EventTriggerResult,
)
from lib.types import Event, ExecutionContext
from kuksa_client.grpc import DataType

log = logging.getLogger("loader")


class LoaderResult(NamedTuple):
    mocked_datapoints: Dict[str, MockedDataPoint]
    behavior_dict: Dict[str, List[Behavior]]


class MockLoader(ABC):
    @abstractmethod
    def load(self, vdb_metadata) -> LoaderResult:
        pass


class PythonDslLoader(MockLoader):
    def _has_supported_type(self, data_type: DataType) -> bool:
        return (
            data_type ==DataType.BOOLEAN
            or data_type ==DataType.FLOAT
            or data_type ==DataType.DOUBLE
            or data_type ==DataType.INT8
            or data_type ==DataType.UINT8
            or data_type ==DataType.INT16
            or data_type ==DataType.UINT16
            or data_type ==DataType.INT32
            or data_type ==DataType.UINT32
            or data_type ==DataType.INT64
            or data_type ==DataType.UINT64
            or data_type ==DataType.STRING
        )

    def _load_mocked_datapoints(self, client) -> Dict[str, MockedDataPoint]:
        mocked_datapoints: Dict[str, MockedDataPoint] = dict()
        for datapoint in _mocked_datapoints:
            metadata = client.get_metadata([datapoint['path'], ])[datapoint['path']]

            if not self._has_supported_type(metadata.data_type):
                log.error(f"Mocked datapoint {datapoint['path']} has unsupported type!")

            mocked_datapoints[datapoint["path"]] = MockedDataPoint(
                datapoint["path"], metadata.data_type, datapoint["initial_value"], True
            )
        return mocked_datapoints

    def _load_behaviors(
        self, client, mocked_datapoints: Dict[str, MockedDataPoint]
    ) -> Tuple[Dict[str, List[Behavior]], Dict[str, MockedDataPoint]]:
        required_datapoints: Dict[str, MockedDataPoint] = dict()
        behavior_dict: Dict[str, List[Behavior]] = dict()

        for datapoint in _mocked_datapoints:
            metadata = client.get_metadata([datapoint['path'], ])[datapoint['path']]

            if self._has_supported_type(metadata.data_type):
                behavior_dict[datapoint["path"]] = list()
                for behavior in datapoint["behaviors"]:
                    behavior_dict[datapoint["path"]].append(behavior)
                    behavior: Behavior = behavior

                    # force execution of condition and action
                    # to identify and register all non-mocked, required datapoints
                    exe_context = ExecutionContext(
                        datapoint["path"], list(), dict(), 0.0
                    )
                    behavior.is_condition_fulfilled(exe_context)

                    trigger = None
                    if behavior.get_trigger_type() == ClockTrigger:
                        trigger = ClockTriggerResult(True)
                    elif behavior.get_trigger_type() == EventTrigger:
                        trigger = EventTriggerResult(
                            True, Event("actuator_target", datapoint["path"], None)
                        )
                    else:
                        raise TypeError("Unsupported TriggerResult type!")
                    behavior.execute(
                        ActionContext(
                            trigger,
                            exe_context,
                            MockedDataPoint(
                                datapoint["path"],
                                mocked_datapoints[datapoint["path"]].data_type,
                                mocked_datapoints[datapoint["path"]].value,
                                True,
                            ),
                        ),
                        list(),
                    )
            else:
                log.error(f"Mocked datapoint {datapoint['path']} has unsupported type!")

        for required_dp in _required_datapoint_paths:
            if required_dp not in mocked_datapoints:
                log.error(f"Creating required datapoint for {required_dp}")
                mocked_datapoint = MockedDataPoint(required_dp, None, None, False)
                required_datapoints[required_dp] = mocked_datapoint

        return behavior_dict, required_datapoints

    def load(self, client) -> LoaderResult:
        """Load mocking configuration from Python script."""
        import importlib.util
        import sys

        spec = importlib.util.spec_from_file_location("mock", "./mock.py")
        mod = importlib.util.module_from_spec(spec)
        sys.modules["mock"] = mod
        spec.loader.exec_module(mod)

        mocked_datapoints = self._load_mocked_datapoints(client)
        behaviors, required_datapoints = self._load_behaviors(
            client, mocked_datapoints
        )

        # convert required datapoints into "normal" mocked datapoints
        # without an initial value
        for key, value in required_datapoints.items():
            if key not in mocked_datapoints:
                mocked_datapoints[key] = value

        return LoaderResult(mocked_datapoints, behaviors)
