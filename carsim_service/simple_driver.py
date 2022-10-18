import logging
import os
import math
import asyncio
import signal
from time import monotonic

from gen_proto.sdv.databroker.v1.types_pb2 import Datapoint
from helper import Databroker

DATABROKER_ADDRESS = os.environ.get("DATABROKER_ADDRESS", "127.0.0.1:55555")

DP_SPEED = "Vehicle.Speed"  # float
DP_ACCEL_LAT = "Vehicle.Acceleration.Lateral"  # float
DP_ACCEL_LONG = "Vehicle.Acceleration.Longitudinal"  # float
DP_ACCEL_VERT = "Vehicle.Acceleration.Vertical"  # float

DP_BRAKE_POS = "Vehicle.Chassis.Brake.PedalPosition"  # uint8
DP_ACCELR_POS = "Vehicle.Chassis.Accelerator.PedalPosition"  # uint8
DP_STEER_ANGLE = "Vehicle.Chassis.SteeringWheel.Angle"  # int16

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "WARN"))


def normal_dist(x , mu , sigma):
    prob_density = (math.pi*sigma) * math.exp(-0.5*((x-mu)/sigma)**2)
    return prob_density


def control_curve(x):
    # positive values - acceleration
    # negative values - braking
    return normal_dist(x, mu=2, sigma=0.5) - 3*normal_dist(x, mu=15, sigma=0.7)


async def setup_helper() -> Databroker:
    logger.info("Using DATABROKER_ADDRESS={}".format(DATABROKER_ADDRESS))
    helper = Databroker(DATABROKER_ADDRESS)
    return helper


# driver that does not steer, only accelerates smoothly (gaussian)
# and decelerates smoothly as well (gaussian)
async def main():
    helper = await setup_helper()

    t0 = monotonic()
    t_max = 20 # seconds
    t_current = monotonic()-t0

    while t_current < t_max:
        control = control_curve(t_current)
        if control > 0:
            pub_accelerator = asyncio.create_task(
            helper.set_uint32_datapoint(DP_ACCELR_POS, int(control*100))
            )
            pub_brake = asyncio.create_task(
            helper.set_uint32_datapoint(DP_BRAKE_POS, 0)
            )
        else:
            pub_accelerator = asyncio.create_task(
            helper.set_uint32_datapoint(DP_ACCELR_POS, 0)
            )
            pub_brake = asyncio.create_task(
            helper.set_uint32_datapoint(DP_BRAKE_POS, int(-control*100))
            )
        
        await pub_accelerator
        await pub_brake
        t_current = monotonic() - t0


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
