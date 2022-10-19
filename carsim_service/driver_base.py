from abc import ABC, abstractmethod


class DriverBase(ABC):
    """
    The skeleton for a basic open-loop driver class. Driver classes that implement
    this interface can only send steering angle, accelerator psition and brake position
    and not receive feedback from the databroker on the car state.
    """
    
    def __init__(self, simulation_speed):
        self.sim_speed = simulation_speed

    @property
    @abstractmethod
    def accelerator_pos(self) -> int:
        """Returns the current accelerator position

        Returns:
            int: uint8 - percent 0 to 100
        """
        raise NotImplemented

    @property
    @abstractmethod
    def brake_pos(self) -> int:
        """Returns the current brake position

        Returns:
            int: uint8 - percent 0 to 100
        """
        raise NotImplemented

    @property
    @abstractmethod
    def steering_angle(self) -> int:
        """Returns the current brake position

        Returns:
            int: int16 - Steering wheel angle.
            Positive = degrees to the left. Negative = degrees to the right.
        """
        raise NotImplemented

    @abstractmethod
    def _update_driver_state(self):
        """Updates the internal state of the driver
        This method should take care that no contradicting values are produced
        e.g. brake = accelerator = 1 and all the units match the VDS
        """
        raise NotImplemented

    def get_controls(self):
        """The main driver-loop method. That first updates the driver state
        and then returns the new state in the form (accelerator, brake, steering_angle).
        """
        self._update_driver_state()
        return (self.accelerator_pos, self.brake_pos, self.steering_angle)
