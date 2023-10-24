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

from typing import Any, List, NamedTuple

from kuksa_client.grpc import VSSClient


class Event(NamedTuple):
    """Structure for holding event data."""

    name: str
    path: str
    value: Any


class ExecutionContext(NamedTuple):
    """Context in which behaviors are executed"""

    calling_signal_path: str
    pending_event_list: List[Event]
    delta_time: float
    client: VSSClient
