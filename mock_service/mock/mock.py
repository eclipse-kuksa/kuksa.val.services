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

from lib.animator import RepeatMode
from lib.dsl import (
    create_animation_action,
    create_behavior,
    create_event_trigger,
    create_set_action,
    get_datapoint_value,
    mock_datapoint,
)
from lib.trigger import ClockTrigger, EventType

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
    path="Vehicle.Body.Windshield.Front.Wiping.System.Mode",
    initial_value="STOP_HOLD",
    behaviors=[
        create_behavior(
            trigger=create_event_trigger(EventType.ACTUATOR_TARGET),
            action=create_set_action("$event.value"),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Body.Windshield.Front.Wiping.System.TargetPosition",
    initial_value=0,
    behaviors=[
        create_behavior(
            trigger=create_event_trigger(EventType.ACTUATOR_TARGET),
            action=create_set_action("$event.value"),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Body.Windshield.Front.Wiping.System.ActualPosition",
    initial_value=0,
    behaviors=[
        create_behavior(
            trigger=create_event_trigger(EventType.ACTUATOR_TARGET),
            condition=lambda ctx: get_datapoint_value(
                ctx, "Vehicle.Body.Windshield.Front.Wiping.System.Mode"
            )
            == "EMERGENCY_STOP",
            action=create_set_action(0),
        ),
        create_behavior(
            trigger=create_event_trigger(EventType.ACTUATOR_TARGET),
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
            trigger=create_event_trigger(EventType.ACTUATOR_TARGET),
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
            create_event_trigger(EventType.ACTUATOR_TARGET),
            create_set_action("$event.value"),
        )
    ],
)

mock_datapoint(
    path="Vehicle.Body.Lights.Brake.IsActive",
    initial_value="INACTIVE",
    behaviors=[
        create_behavior(
            trigger=create_event_trigger(
                EventType.VALUE, "Vehicle.Body.Lights.Brake.IsDefect"
            ),
            action=create_set_action("INACTIVE"),
        ),
        create_behavior(
            trigger=create_event_trigger(
                EventType.ACTUATOR_TARGET, "Vehicle.Body.Lights.Brake.IsActive"
            ),
            action=create_set_action("$event.value"),
        ),
    ],
)
