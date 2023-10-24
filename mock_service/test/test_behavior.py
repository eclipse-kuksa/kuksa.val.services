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
from lib.action import SetAction
from lib.behavior import Behavior
from lib.trigger import ClockTrigger, EventTrigger, EventType
from lib.types import ExecutionContext


def test_condition_function_true():
    cut = Behavior(ClockTrigger(0), condition=lambda _: True, action=SetAction(0))
    context = ExecutionContext(None, None, 0.0, None)

    assert cut.is_condition_fulfilled(context)


def test_condition_function_false():
    cut = Behavior(ClockTrigger(0), condition=lambda _: False, action=SetAction(0))
    context = ExecutionContext(None, None, 0.0, None)

    assert not cut.is_condition_fulfilled(context)


def test_aaa():
    cut1 = Behavior(ClockTrigger(0), condition=lambda _: False, action=SetAction(0))
    cut2 = Behavior(
        EventTrigger(EventType.ACTUATOR_TARGET),
        condition=lambda _: False,
        action=SetAction(0),
    )

    assert cut1.get_trigger_type() == ClockTrigger
    assert cut2.get_trigger_type() == EventTrigger
