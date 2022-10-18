import logging
import os
import math
import asyncio
import signal
import grpc

from aioretry import (
    retry,
    # Tuple[bool, Union[int, float]]
    RetryPolicyStrategy,
    RetryInfo
)

from gen_proto.sdv.databroker.v1.types_pb2 import Datapoint
from helper import Databroker
from bicycle_sim import SimulatedCar


DATABROKER_ADDRESS = os.environ.get("DATABROKER_ADDRESS", "127.0.0.1:55555")

DP_SPEED = "Vehicle.Speed"  # float
DP_ACCEL_LAT = "Vehicle.Acceleration.Lateral"  # float
DP_ACCEL_LONG = "Vehicle.Acceleration.Longitudinal"  # float
DP_ACCEL_VERT = "Vehicle.Acceleration.Vertical"  # float

DP_BRAKE_POS = "Vehicle.Chassis.Brake.PedalPosition"  # uint8
DP_ACCELR_POS = "Vehicle.Chassis.Accelerator.PedalPosition"  # uint8
DP_STEER_ANGLE = "Vehicle.Chassis.SteeringWheel.Angle"  # int16

KUKSA_QUERY = "SELECT {}, {}, {}".format(DP_BRAKE_POS, DP_ACCELR_POS, DP_STEER_ANGLE)

SIM_SPEED = float(os.environ.get("SIM_SPEED", 2))  # timeout between updates

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "INFO"))

helperlogger = logging.getLogger("helper")
helperlogger.setLevel(os.getenv("LOG_LEVEL", "WARN"))

# This example shows the usage with python typings
def _retry_policy(info: RetryInfo) -> RetryPolicyStrategy:
    #return False, (info.fails - 1) % 3 * 0.1
    return False, (info.fails - 1) % 3 * 2

async def _before_retry(info: RetryInfo) -> None:
    if isinstance(info.exception, (KeyError, ValueError, KeyboardInterrupt)):
        # No retry for KeyError
        return True, 0
    elif isinstance(info.exception, grpc.aio._call.AioRpcError):
        # Do something
        logger.warn("Retry grpc... %s, %s", info.fails, info.exception.code())
    else:
        logger.warn("Retry... %s, Detailed exception: %s", info.fails, info.exception)
        
async def setup_helper() -> Databroker:
    logger.info("Using DATABROKER_ADDRESS={}".format(DATABROKER_ADDRESS))
    helper = Databroker(DATABROKER_ADDRESS)
    return helper

# TODO: Actually, we (carsim app) are not allowed to set these datapoints, as they will be registerd
# by the driver app. Remove the flush_controls()
@retry(retry_policy=_retry_policy, before_retry=_before_retry)
async def flush_controls(helper):
    await asyncio.create_task(helper.set_uint32_datapoint(DP_BRAKE_POS, 0))
    await asyncio.create_task(helper.set_uint32_datapoint(DP_ACCELR_POS, 0))
    await asyncio.create_task(helper.set_uint32_datapoint(DP_STEER_ANGLE, 0))
    logger.debug(f"flushed {DP_BRAKE_POS} {DP_ACCELR_POS} {DP_STEER_ANGLE}")

@retry(retry_policy=_retry_policy, before_retry=_before_retry)
async def mainloop(car_sim, helper, set_model_param):
        controls_update = asyncio.create_task(
            helper.subscribe_datapoints(
                KUKSA_QUERY, timeout=SIM_SPEED, sub_callback=set_model_param
            )
        )
        await controls_update
        car_sim.update_car()

        pub_speed = asyncio.create_task(
            helper.set_float_datapoint(
                DP_SPEED, car_sim.speed * 3.6
            )  # 1 m/s = 3.6 km/h
        )
        pub_accel_long = asyncio.create_task(
            helper.set_float_datapoint(DP_ACCEL_LONG, car_sim.acceleration[0])  # m/s^2
        )
        pub_accel_lat = asyncio.create_task(
            helper.set_float_datapoint(DP_ACCEL_LAT, car_sim.acceleration[1])  # m/s^2
        )
        pub_accel_vert = asyncio.create_task(
            helper.set_float_datapoint(DP_ACCEL_LONG, 0.0)  # m/s^2
        )

        await pub_speed
        await pub_accel_lat
        await pub_accel_long
        await pub_accel_vert
        logger.debug(f"Car State: {car_sim.__dict__}")
        logger.info(f"Time={car_sim.simul_time} Speed={car_sim.speed * 3.6} km/h Acceleration (Long)={car_sim.acceleration[0]} m/s^2 Acceleration (Lat)={car_sim.acceleration[1]} m/s^2")
        
        #max_acceleration=6.0,   # m/s^2
        #max_deceleration=10.0,  # m/s^2
        #max_speed=60.0,         # m/s

    
async def main():
    helper = await setup_helper()
    car_sim = SimulatedCar()

    def set_model_param(name: str, dp_raw: Datapoint) -> None:
        logger.debug(f'{name=}, {dp_raw=}')
        dp = helper.datapoint_to_dict(name, dp_raw)

        if dp["type"] == "failure_value":
            value = 1
        else:
            value = dp["value"]

        if name == DP_BRAKE_POS:
            car_sim.brake_position = value / 100  # percent
            return
        if name == DP_ACCELR_POS:
            car_sim.accelerator_position = value / 100  # percent
            return
        if name == DP_STEER_ANGLE:
            car_sim.steer_angle = value * math.pi / 180  # deg too rad
            return
    
    await flush_controls(helper)
    
    
    # the databroker should probably be flushed to avoid updates with stale data
    while True:
        await mainloop(car_sim, helper, set_model_param)

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
 