import asyncio
import logging
import math
from operator import add
import os
import signal
from time import monotonic

import grpc
from aioretry import RetryInfo, RetryPolicyStrategy, retry
from helper import Databroker

DATABROKER_ADDRESS = os.environ.get("DATABROKER_ADDRESS", "127.0.0.1:55555")

DP_BRAKE_POS = "Vehicle.Chassis.Brake.PedalPosition"  # uint8
DP_ACCELR_POS = "Vehicle.Chassis.Accelerator.PedalPosition"  # uint8
DP_STEER_ANGLE = "Vehicle.Chassis.SteeringWheel.Angle"  # int16

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "INFO"))

helperlogger = logging.getLogger("helper")
helperlogger.setLevel(os.getenv("LOG_LEVEL", "CRITICAL"))

rootlogger = logging.getLogger("root")
rootlogger.setLevel(os.getenv("LOG_LEVEL", "CRITICAL"))

grpclogger = logging.getLogger("grpc")
grpclogger.setLevel(os.getenv("LOG_LEVEL", "CRITICAL"))

SIM_SPEED = float(os.environ.get("SIM_SPEED", 0.5))  # timeout between updates


# This example shows the usage with python typings
def _retry_policy(info: RetryInfo) -> RetryPolicyStrategy:
    # return False, (info.fails - 1) % 3 * 0.1
    return False, (info.fails - 1) % 3 * 2


async def _before_retry(info: RetryInfo) -> None:
    if isinstance(info.exception, (KeyError, ValueError, KeyboardInterrupt)):
        # No retry for KeyError
        return True, 0
    elif isinstance(info.exception, grpc.aio._call.AioRpcError):
        # Do something
        logger.warning("Retry grpc... %s, %s", info.fails, info.exception.code())
    else:
        logger.warning(
            "Retry... %s, Detailed exception: %s", info.fails, info.exception
        )


def normal_dist(x, mu, sigma):
    prob_density = (math.pi * sigma) * math.exp(-0.5 * ((x - mu) / sigma) ** 2)
    return prob_density


def control_curve(x):
    # positive values - acceleration
    # negative values - braking
    return normal_dist(x, mu=2, sigma=0.5) - 3 * normal_dist(x, mu=15, sigma=0.7)


async def setup_helper() -> Databroker:
    logger.info("Using DATABROKER_ADDRESS={}".format(DATABROKER_ADDRESS))
    helper = Databroker(DATABROKER_ADDRESS)
    return helper


# driver that does not steer, only accelerates smoothly (gaussian)
# and decelerates smoothly as well (gaussian)
@retry(retry_policy=_retry_policy, before_retry=_before_retry)
async def main_loop(helper, t_current):
    control = control_curve(t_current)
    if control > 0:
        pub_accelerator = asyncio.create_task(
            helper.set_uint32_datapoint(DP_ACCELR_POS, int(control * 100))
        )
        pub_brake = asyncio.create_task(helper.set_uint32_datapoint(DP_BRAKE_POS, 0))
    else:
        pub_accelerator = asyncio.create_task(
            helper.set_uint32_datapoint(DP_ACCELR_POS, 0)
        )
        pub_brake = asyncio.create_task(
            helper.set_uint32_datapoint(DP_BRAKE_POS, int(-control * 100))
        )

    await pub_accelerator
    await pub_brake
    

async def main():
    simul_timestep = SIM_SPEED
    t = 0
    t_max = 20
    
    helper = await setup_helper() 
    while t < t_max:
        await main_loop(helper, t)
        t += simul_timestep
        await asyncio.sleep(SIM_SPEED)
    await helper.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
