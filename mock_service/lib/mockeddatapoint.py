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

from typing import Optional, Callable, Any, List

from kuksa_client.grpc import DataType
from lib.datapoint import DataPoint
from lib.behavior import Behavior


class MockedDataPoint:
    """Represents a mocked datapoint that was loaded and programmed with behaviors."""

    def __init__(
        self,
        path: str,
        data_type: DataType,
        value: Any,
        is_mocked: bool,
        behaviors: List[Behavior] = list(),
        value_listener: Optional[Callable[[Any], None]] = None,
    ):
        self.datapoint = DataPoint(path, data_type, value, value_listener)

        self.is_mocked = is_mocked
        self.behaviors = behaviors

    def __eq__(self, other):
        return (
            isinstance(other, MockedDataPoint)
            and self.behaviors == other.behaviors
        )

    def __ne__(self, other):
        return not self.__eq__(other)
