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

from typing import Any, Callable, Optional

from sdv.databroker.v1.types_pb2 import DataType


class MockedDataPoint:
    """Represents a mocked datapoint that was loaded and programmed with behaviors."""

    def __init__(
        self,
        path: str,
        data_type: DataType,
        value: Any,
        is_mocked: bool,
        value_listener: Optional[Callable[[Any], None]] = None,
    ):
        self.path = path
        self.data_type = data_type
        self.value = value
        self.is_mocked = is_mocked
        self.value_listener = value_listener

    def has_discrete_value_type(self):
        """Return if the datapoint has a discrete value type."""
        return (
            DataType.Name(self.data_type) == "BOOL"
            or DataType.Name(self.data_type) == "STRING"
        )

    def set_value(self, new_value):
        """Set the value of the datapoint."""
        if self.value != new_value:
            self.value = new_value
            if self.value_listener is not None:
                self.value_listener(self)
