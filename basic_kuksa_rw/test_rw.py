
import json
import logging
import os

import asyncio
import signal 

from gen_proto.sdv.databroker.v1.types_pb2 import Datapoint
from helper import Databroker

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "WARN"))
DATABROKER_ADDRESS = os.environ.get("DATABROKER_ADDRESS", "127.0.0.1:55555")

async def setup_helper() -> Databroker:
    logger.info("Using DATABROKER_ADDRESS={}".format(DATABROKER_ADDRESS))
    helper = Databroker(DATABROKER_ADDRESS)
    return helper

async def run_carsim() -> None:
    helper: Databroker = await setup_helper()

    timeout = 3
    datapoint_speed = "Vehicle.OBD.Speed" # float
    alias_speed = "speed"

    query = "SELECT {} as {}".format(datapoint_speed, alias_speed)

    events = []
    # inner function for collecting subscription events

    def inner_callback(name: str, dp: Datapoint):
        dd = helper.datapoint_to_dict(name, dp)
        events.append(dd)

    logger.info("# subscribing('{}', timeout={})".format(query, timeout))

    subscription = asyncio.create_task(
        helper.subscribe_datapoints(query, timeout=timeout, sub_callback=inner_callback)
    )

    set_speed = asyncio.create_task(
        helper.set_float_datapoint(datapoint_speed, 40.0)
    )
    
    set_speed = asyncio.create_task(
        helper.set_float_datapoint(datapoint_speed, 80.0)
    )
    await set_speed
    await subscription
    print(events)
    logger.debug("Received events:{}".format(events))

    await helper.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(run_carsim())
    LOOP.close()
