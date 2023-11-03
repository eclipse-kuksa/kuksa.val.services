import threading
import os
import time
import logging

from mock.mockservice import MockService
from lib.animator import RepeatMode
from lib.dsl import (
    create_animation_action,
    create_behavior,
    mock_datapoint,
    _mocked_datapoints,
)
from lib.trigger import ClockTrigger

# Configure the root logger
root_log = logging.getLogger()
root_log.setLevel(logging.DEBUG)

# Configure a console handler to display log messages on the command line
console_handler = logging.StreamHandler()
console_handler.setLevel(logging.DEBUG)

# Create a formatter with your desired log message format
formatter = logging.Formatter('%(asctime)s [%(threadName)s] [%(name)s] [%(levelname)s] %(message)s')
console_handler.setFormatter(formatter)

# Add the console handler to the root logger
root_log.addHandler(console_handler)

# Rest of your code
MOCK_ADDRESS = os.getenv("MOCK_ADDR", "0.0.0.0:50053")

if __name__ == "__main__":
    mock = MockService(MOCK_ADDRESS)

    # Set the logger for the mock instance to the root logger
    mock.log = root_log
    threading.Thread(target=mock.main_loop).start()
    print(_mocked_datapoints)
    time.sleep(10)
    mock_datapoint(
        path="Vehicle.Width",
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
    print(_mocked_datapoints)
