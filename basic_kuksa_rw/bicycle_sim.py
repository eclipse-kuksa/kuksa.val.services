import numpy as np
from time import monotonic


class SimulatedCar:
    """
    A simple (black-box) car kinematics simulator. It takes as input:
    accelerator position (0, 1), brake position (0,1)
    and steering angle (-pi/2, pi/2).
    It is assumed that braking and accelerating are two antagonistic
    effects that can't happen at the same time and that they
    are related linearly to the acceleration
    (this models the "jerk" when driving a car).

    This assumption is then used in combination with the bicycle vehicle model
    (see: https://thef1clan.com/2020/09/21/vehicle-dynamics-the-kinematic-bicycle-model/)
    to propagate the simulated car in 2D space.
    After each propagation step velocity and acceleration componets are calculated.

    Example:
    ----------
    >>> from simulator import SimulatedCar
    >>> c = SimulatedCar()
    >>> c.steer_angle = math.pi/4
    >>> c.accelerator_position = 0.2
    >>> while True:
    >>>     c.update_car()
    >>>     print(f"{car.position=}")

    Note: steer_angle, accelerator position and brake_position can be
    changed at any time and will affect the next call of the
    update_car() method.
    """

    def __init__(
        self,
        max_acceleration=6.0,   # m/s^2
        max_deceleration=10.0,  # m/s^2
        max_speed=60.0,         # m/s
        drag_coef=0.0,          # 1/s
        simulation_step=1e-3,   # s
        car_len=2.0,            # m
        lr=1.0,                 # m, distance between rear axle and COG
    ):

        # simulation constants
        self._max_acceleration = max_acceleration
        self._max_decleration = max_deceleration
        self._max_speed = max_speed
        self._drag_coef = drag_coef
        self._simulation_step = simulation_step
        self._carl_len = car_len
        self._lr = lr

        # control variables:
        self._steer_angle = 0.0  # radians [-pi/2, pi/2]
        self._control_pos = 0.0  # dimensionless [-1, 1]

        # internal car state
        self._simul_time = 0.0              # s
        self._sys_timestamp = monotonic()   # s
        self._heading_angle = 0.0           # radians
        self._x_pos = 0.0                   # m
        self._y_pos = 0.0                   # m
        self._v_x = 0.0                     # m/s
        self._v_y = 0.0                     # m/s
        self._speed = 0.0                   # m/s
        self._accel_x = 0.0                 # m/s^2
        self._accel_y = 0.0                 # m/s^2

        # these two are needed in order to calculate the components of
        # the acceleration at the end of the update cycle
        self._old_v_x = self._v_x
        self._old_v_y = self._v_y

    # Setup r/w access to the interal state,
    # returning vector quantities as (x,y) tuples
    # and scalars as a single value
    @property
    def steer_angle(self):
        return self._steer_angle

    @steer_angle.setter
    def steer_angle(self, value):
        self._steer_angle = value

    @property
    def brake_position(self):
        if self._control_pos > 0:
            return 0
        else:
            return -self._control_pos

    @brake_position.setter
    def brake_position(self, value):
        self._control_pos = -value

    @property
    def accelerator_position(self):
        if self._control_pos < 0:
            return 0
        else:
            return self._control_pos

    @accelerator_position.setter
    def accelerator_position(self, value):
        self._control_pos = value

    # read-only properties
    @property
    def simul_time(self):
        return self._simul_time

    @property
    def sys_timestamp(self):
        return self._sys_timestamp

    @property
    def heading_angle(self):
        return self._heading_angle

    @property
    def position(self):
        return (self._x_pos, self._y_pos)

    @property
    def velocity(self):
        return (self._v_x, self._v_y)

    @property
    def speed(self):
        return self._speed

    @property
    def acceleration_world(self):
        return (self._accel_x, self._accel_y)

    @property
    def acceleration(self):
        # get the acceleration in path (local) coordinates as if read by accelerometer
        a  = np.array([self._accel_x,self._accel_y])
        v = np.array([self._v_x, self._v_y])
        aT = np.dot(v, a)/np.linalg.norm(v,2) # longitudinal (tangential) component
        aN = np.cross(v, a)/np.linalg.norm(v,2) # lateral (normal) component

        return float(aT), float(aN) # convert to normal python floats from numpy.float64

    @property
    def acceleration_norm(self):
        return np.sqrt(self._accel_x**2 + self._accel_y**2)

    def _acc_from_ctrl(self):
        """Gas and brake are combined in a single variable 
        "control position" that can take any value in the interval [-1,1].
        An implicit assumption here is that you cannot press the gas
        and the brake at the same time.
        Where ctrl_position  > 0 implies acceleration and
        ctrl_position < 0 implies braking.
        A pieciewise linear relation betwen the ctrl_position and the
        "signed norm" of the acceleration
        (> 0 - acceleration, < 0 - braking)
        is constructed based on the values
        for max_acceleration and max_decleration provided at initialization.

        Args:
            ctrl_position (float): The control position

        Returns:
            float: The acceleration calculated from the control position
        """
        if self._control_pos >= 0:
            return self._max_acceleration * min(self._control_pos, 1)
        else:
            return self._max_decleration * max(self._control_pos, -1)

    def _cog_turning_angle(self):
        # The turning angle of the center of gravity.
        # See the angle "beta" in the reference text.
        return np.arctan(self._lr * np.tan(self._steer_angle) / self._carl_len)

    def _rotational_speed(self):
        return (
            self._speed
            * (np.tan(self._steer_angle) * np.cos(self._cog_turning_angle()))
            / self._carl_len
        )

    def _update_speed(self):
        # update speed assuming locally
        # linear behaviour of acceleration and drag
        new_speed = (
            self._speed
            + (self._acc_from_ctrl() - self._drag_coef * self._speed)
            * self._simulation_step
        )
        self._speed = max(0, min(new_speed, self._max_speed))

    def _update_heading_angle(self):
        self._heading_angle = (
            self._heading_angle + self._rotational_speed() * self._simulation_step
        )

    def _update_velocity(self):
        self._old_v_x = self._v_x
        self._old_v_y = self._v_y

        self._v_x = self._speed * np.cos(
            self.heading_angle + self._cog_turning_angle()
        )
        self._v_y = self._speed * np.sin(
            self.heading_angle + self._cog_turning_angle()
        )

    def _update_position(self):
        self._x_pos = self._x_pos + self._v_x * self._simulation_step
        self._y_pos = self._y_pos + self._v_y * self._simulation_step

    def _update_acceleration(self):
        # Acceleration as measured by an internal car acclerometer
        self._accel_x = (self._v_x - self._old_v_x) / (self._simulation_step)
        self._accel_y = (self._v_y - self._old_v_y) / (self._simulation_step)

    def update_car(self):
        # the order of the updates here is important,
        # it should be:
        # simul time -> speed -> heading -> velocity -> position -> acceleration -> sys timestamp
        self._simul_time = self._simul_time + self._simulation_step
        self._update_speed()
        self._update_heading_angle()
        self._update_velocity()
        self._update_position()
        self._update_acceleration()
        self._sys_timestamp = monotonic()


def test_data():
    # this should simulate going in circles
    car = SimulatedCar()
    car.steer_angle = np.pi/10
    car.accelerator_position = 1
    for i in range(100_000):
        car.update_car()
        print(f"{car.position[0]}\t{car.position[1]}")

if __name__ == "__main__":
    test_data()
