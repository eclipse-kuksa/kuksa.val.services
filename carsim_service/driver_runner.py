import asyncio
import logging
import os
import traceback

import grpc
from aioretry import RetryInfo, RetryPolicyStrategy, retry
from driver import Driver
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

SIM_SPEED = float(os.environ.get("SIM_SPEED", 2))  # timeout between updates


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
            "Retry... %s, Detailed exception: %s. \n Traceback: %s",
            info.fails,
            info.exception,
            traceback.format_exc(),
        )


async def setup_helper() -> Databroker:
    logger.info("Using DATABROKER_ADDRESS={}".format(DATABROKER_ADDRESS))
    helper = Databroker(DATABROKER_ADDRESS)
    return helper


@retry(retry_policy=_retry_policy, before_retry=_before_retry)
async def main_loop(helper: Databroker, driver: Driver):
    accelerator, brake, steering_angle = driver.get_controls()
    logger.debug(f"Sending {accelerator=} {brake=} {steering_angle=}")
    await asyncio.create_task(helper.set_uint32_datapoint(DP_ACCELR_POS, accelerator))
    await asyncio.create_task(helper.set_uint32_datapoint(DP_BRAKE_POS, brake))
    await asyncio.create_task(helper.set_int32_datapoint(DP_STEER_ANGLE, steering_angle))
    await asyncio.sleep(SIM_SPEED)


async def main():
    helper = await setup_helper()
    driver = Driver(SIM_SPEED)

    with await setup_helper() as helper:
        while True:
            await main_loop(helper, driver)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
