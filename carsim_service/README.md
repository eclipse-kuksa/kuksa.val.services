# CAR sim example

The CAR sim service is a service which simulates a vehicle's basic physical movement, such as acceleration, speed and steering.

## Usage

1. Run Eclipse Kuksa.VAL Data Broker

    docker run --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master

2. Run the CAR simulator

    python3 carsim.py

3. Run the DRIVER  simulator

    python3 driversim.py

# CAR sim

## Vehicle Signals

As a *Vehicle Service*, the CAR sim is supposed to update sensor data in the Vehicle Signal Specification tree. A real implementation would read sensor data via hardware/physical sensors while the CAR sim is simulating the sensor data. It is implemented as a Kuksa Feeder, interacting with the Kuksa Data Broker only. It is supposed to be deployed as a headless container running on the device.

### Input

```
Vehicle.Chassis.Accelerator.PedalPosition : uint8 in percent
Vehicle.Chassis.Break.PedalPosition : uint8 in percent
Vehicle.Chassis.SteeringWheel.Angle : int16 in degrees (positive=left, negative=right)
Vehicle.CurrentOverallWeight : uint16 in kg
```

#### Acceleration, Break and Steering

These three values are the input for the CAR simulation. They will be provided by another Vehicle Service called `driversim` (see below).

#### Vehicle Weight

*Not to be implemented for now*
The CAR sim will set the `Vehicle.CurrentOverallWeight` initially to a static value of `1500 kg`. It can not be changed.

### Output

The following vehicle signals or sensor values are calculated by the CAR sim algorithm and updated in a stream of vehicle signals into the Kuksa Data Broker:

```
Vehicle.Acceleration.Lateral : float in m/s^3
Vehicle.Acceleration.Longitudinal : float in m/s^3
Vehicle.Acceleration.Vertical : float in m/s^3
Vehicle.Speed : float in km/h
```

# Driver Sim App

For the physical simulation of the vehicle, the model needs to have input as well, otherwise it would only be reflecting a parked car, which would be very boring.

The driver sim app simulates a "human" driver with the following behavior:
- Driver simulator start with a "good" driving style:
    - He starts to drive around slowly, that means with low acceleration and low speeds
    - Every 10 seconds, he changes the acceleration to reach a new target speed, which randomly selects either 30, 50 or 70 km/h
            -> Requires https://en.wikipedia.org/wiki/PID_controller   == Cruise Control
            Alternatibve: He changes the accleration randomly without knowing current speed?
    - Every 20 seconds, he randomly steers to the left or to the right, smoothly with large turning radius
- After 5 minutes of driving, the virtual driver switches to a "bad" driving style:
    - He increases acceleration to a maximum until he is reaching maximum speed of 200 km/h
    - Every 10 seconds, he changes the acceleration to reach a new target speed, which randomly selects either 30km/h, 100km/h or 200km/h
    - Every 15 seconds, he randomly steers to the left or to the right harshly with small turning radius.
    - Every 20 seconds, the break pedal position is pressed for 1 second in a random position between 50% and 100%
- Every 5 minutes, the driver sim changes the driving style between "good" and "bad".

