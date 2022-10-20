from driver_base import DriverBase
import math
from time import sleep
import random


class Driver(DriverBase):
    def __init__(self, simulation_speed=2.0):  # time scale of sim in seconds
        super().__init__(simulation_speed)
        self._simulation_time = 0
        self._good_driving_cycles = 0
        self._good_driving_time = 400 * self.simulation_speed  # seconds

        self._bad_driving_cycles = 0
        self._bad_driving_time = 400 * self.simulation_speed  # seconds

        self._is_good = True

        self._brake = 0
        self._accelerator = 0
        self._steer_angle = 0

    def recalculate_time(self):
        t = (
            self._simulation_time
            - self._good_driving_cycles * self._good_driving_time
            - self._bad_driving_cycles * self._bad_driving_time
        )  # proof needed!

        return t

    def calculate_good_driver(self):
        """
        Accelerate with a gaussian, peaking at max_accel_t and
        decelerate with a gaussian, peaking at max_decel_t (rescaled to sim
        speed)
        for 0 < t <  turn_time turn slowly between 0 and 20 degrees
        for turn_time < t < 4*turn_time turn back between 20 and 0 degrees
        """

        # rescale gaussians based on simulation speed
        sigma_acc = 20 * self.simulation_speed
        sigma_decel = 10 * self.simulation_speed
        max_accel_t = 50 * self.simulation_speed
        max_decel_t = 300 * self.simulation_speed
        turn_time = 50 * self.simulation_speed
        turn_angle_max = 20
        angle_step = 0.5

        t = self.recalculate_time()

        if t > self._good_driving_time:
            self._is_good = False
            self._good_driving_cycles += 1
            return

        control = math.exp(-(((t - max_accel_t) / sigma_acc) ** 2)) - math.exp(
            -(((t - max_decel_t) / sigma_decel) ** 2)
        )

        if control > 0:
            self._accelerator = control
            self._brake = 0
        else:
            self._accelerator = 0
            self._brake = -control

        if 0 < t < 2 * turn_time:
            new_angle = self._steer_angle + angle_step
            self._steer_angle = min(new_angle, turn_angle_max)
        elif turn_time < t < 4 * turn_time:
            new_angle = self._steer_angle - angle_step
            self._steer_angle = max(new_angle, 0)
        else:
            self._steer_angle = 0

    def calculate_bad_driver(self):
        """
        Presses the accelerator to the max for 1/4 of the bad driving time
        brake for 1/4 to 2/4, accelerator from 2/4 to 3/4, and brake again
        from 3/4 to 4/4
        Randomly steer between -5 and 5 degrees
        """
        t = self.recalculate_time()

        if t > self._bad_driving_time:
            self._is_good = True
            self._bad_driving_cycles += 1
            return

        if 0 <= t < 0.25 * self._bad_driving_time:
            self._accelerator = 1
            self._brake = 0
            self._steer_angle = random.randint(-5, 5)

        elif 0.25 * self._bad_driving_time <= t < 0.5 * self._bad_driving_time:
            self._accelerator = 0
            self._brake = 1
            self._steer_angle = random.randint(-5, 5)

        elif 0.50 * self._bad_driving_time <= t < 0.75 * self._bad_driving_time:
            self._accelerator = 1
            self._brake = 0
            self._steer_angle = random.randint(-5, 5)

        elif 0.50 * self._bad_driving_time <= t < self._bad_driving_time:
            self._accelerator = 0
            self._brake = 1
            self._steer_angle = random.randint(-5, 5)

    @property
    def accelerator_pos(self) -> int:
        return int(self._accelerator * 100)

    @property
    def brake_pos(self) -> int:
        return int(self._brake * 100)

    @property
    def steering_angle(self) -> int:
        return int(self._steer_angle)

    @property
    def simulation_time(self) -> float:
        return self._simulation_time

    def _update_driver_state(self):
        self._simulation_time += self.simulation_speed

        if self._is_good:
            self.calculate_good_driver()
        else:
            self.calculate_bad_driver()


def main():
    d = Driver(2)
    print("time\tacceleration\tbrake\tsteer\tcontrol")
    while d._good_driving_cycles < 3:
        a, b, s = d.get_controls()
        print(f"{d.simulation_time}\t{a}\t{b}\t{s}\t{a-b}")


if __name__ == "__main__":
    main()
