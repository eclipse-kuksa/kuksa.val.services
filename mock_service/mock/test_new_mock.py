from lib.dsl import (
    create_animation_action,
    create_behavior,
    create_set_action,
    get_datapoint_value,
    mock_datapoint,
)
from lib.trigger import ClockTrigger, EventTrigger, EventType

from lib.animator import RepeatMode

if __name__ == "__main__":
    mock_datapoint(
        path="Vehicle.Cabin.Seat.Row1.Pos1.Position",
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