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

from typing import Any, Dict, List, NamedTuple

from lib.datapoint import MockedDataPoint
from lib.baseservice import VDB_ADDRESS

from kuksa_client.grpc import VSSClient

vdb_split = VDB_ADDRESS.split(':')

class Event(NamedTuple):
    """Structure for holding event data."""

    name: str
    path: str
    value: Any


class ExecutionContext(NamedTuple):
    """Context in which behaviors are executed"""

    calling_signal_path: str
    pending_event_list: List[Event]
    client = VSSClient(vdb_split[0], vdb_split[1])
    client.connect()
    delta_time: float
