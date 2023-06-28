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

from lib.dsl import (
    create_animation_action,
    create_behavior,
    create_set_action,
    get_datapoint_value,
    mock_datapoint,
)
from lib.trigger import ClockTrigger, EventTrigger, EventType

from lib.animator import RepeatMode

mock_datapoint(
    path="Vehicle.Speed",
    initial_value=0.0,
    behaviors=[
        create_behavior(
            trigger=ClockTrigger(0),
            action=create_animation_action(
                duration=10.0,
                repeat_mode=RepeatMode.REPEAT,
                values=[0, 30.0, 50.0, 70.0, 100.0, 70.0, 50.0, 30.0, 0.0],
            ),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Cabin.Seat.Row1.Pos1.Position",
    initial_value=0,
    behaviors=[
        create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            action=create_animation_action(
                duration=10.0,
                values=["$self", "$event.value"],
            ),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Body.Windshield.Front.Wiping.System.Mode",
    initial_value="STOP_HOLD",
    behaviors=[
        create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            action=create_set_action("$event.value"),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Body.Windshield.Front.Wiping.System.TargetPosition",
    initial_value=0,
    behaviors=[
        create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            action=create_set_action("$event.value"),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Body.Windshield.Front.Wiping.System.ActualPosition",
    initial_value=0,
    behaviors=[
        create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            condition=lambda ctx: get_datapoint_value(
                ctx, "Vehicle.Body.Windshield.Front.Wiping.System.Mode"
            )
            == "EMERGENCY_STOP",
            action=create_set_action(0),
        ),
        create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            condition=lambda ctx: get_datapoint_value(
                ctx, "Vehicle.Body.Windshield.Front.Wiping.System.Mode"
            )
            == "STOP_HOLD",
            action=create_animation_action(
                duration=10.0,
                values=[
                    "$self",
                    "$Vehicle.Body.Windshield.Front.Wiping.System.TargetPosition",
                ],
            ),
        ),
        create_behavior(
            trigger=EventTrigger(EventType.ACTUATOR_TARGET),
            condition=lambda ctx: get_datapoint_value(
                ctx, "Vehicle.Body.Windshield.Front.Wiping.System.Mode"
            )
            == "WIPE",
            action=create_animation_action(
                duration=10.0,
                values=[
                    "$self",
                    "$Vehicle.Body.Windshield.Front.Wiping.System.TargetPosition",
                ],
            ),
        ),
    ],
)

mock_datapoint(
    path="Vehicle.Cabin.HVAC.IsFrontDefrosterActive",
    initial_value=False,
    behaviors=[
        create_behavior(
            EventTrigger(EventType.ACTUATOR_TARGET), create_set_action("$event.value")
        )
    ],
)
