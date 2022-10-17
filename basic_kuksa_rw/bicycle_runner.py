import logging
import os
import math
import asyncio
import signal

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

SIM_SPEED = float(os.environ.get("SIM_SPEED", 0.001))  # timeout between updates

logger = logging.getLogger(__name__)
logger.setLevel(os.getenv("LOG_LEVEL", "WARN"))


async def setup_helper() -> Databroker:
    logger.info("Using DATABROKER_ADDRESS={}".format(DATABROKER_ADDRESS))
    helper = Databroker(DATABROKER_ADDRESS)
    return helper


async def flush_controls(helper):
    await asyncio.create_task(helper.set_uint32_datapoint(DP_BRAKE_POS, 0))
    await asyncio.create_task(helper.set_uint32_datapoint(DP_ACCELR_POS, 0))
    await asyncio.create_task(helper.set_uint32_datapoint(DP_STEER_ANGLE, 0))
    logger.debug(f"flushed {DP_BRAKE_POS} {DP_ACCELR_POS} {DP_STEER_ANGLE}")


async def main():
    helper = await setup_helper()
    car_sim = SimulatedCar()

    def set_model_param(name: str, dp_raw: Datapoint) -> None:
        dp = helper.datapoint_to_dict(name, dp_raw)

        if dp["type"] == "failure_value":
            return

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


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    LOOP = asyncio.get_event_loop()
    LOOP.add_signal_handler(signal.SIGTERM, LOOP.stop)
    LOOP.run_until_complete(main())
    LOOP.close()
