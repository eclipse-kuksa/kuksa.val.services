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
from typing import List, Dict

from lib.types import Event, ExecutionContext
from lib.mockeddatapoint import MockedDataPoint
from lib.action import ActionContext
from kuksa_client.grpc import VSSClient

SERVICE_NAME = "mock_service"

log = logging.getLogger(SERVICE_NAME)


class BehaviorExecutor:
    """Manager/executor for all behaviors."""

    def __init__(
        self,
        mocked_datapoints: Dict[str, MockedDataPoint],
        pending_event_list: List[Event],
        client: VSSClient,
    ):
        self._mocked_datapoints = mocked_datapoints
        self._pending_event_list = pending_event_list
        self._client = client

    def execute(self, delta_time: float):
        """Executes all behaviors in order given that their trigger has activated and their respective conditions are met."""

        for path, element in self._mocked_datapoints.items():
            for behavior in element.behaviors:
                execution_context = ExecutionContext(
                    path, self._pending_event_list, delta_time, self._client
                )
                if behavior.is_condition_fulfilled(
                        execution_context
                ):
                    trigger_result = behavior.check_trigger(execution_context)
                    if trigger_result.is_active():
                        log.info(f"Running behavior for {path}")
                        behavior.execute(
                            ActionContext(
                                trigger_result, execution_context, element.datapoint
                            ),
                        )
                        break
