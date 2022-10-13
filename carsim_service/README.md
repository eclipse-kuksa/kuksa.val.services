# CAR sim example

The CAR sim service is a service which simulates a vehicle's basic physical movement, such as acceleration, speed and steering.

## Usage

1. Run Eclipse Kuksa.VAL Data Broker

    docker run --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master

2. Run the CAR simulator

    python3 carsim.py

## Vehicle Signals

As a *Vehicle Service*, the CAR sim is supposed to update sensor data in the Vehicle Signal Specification tree. A real implementation would read sensor data via hardware/physical sensors while the CAR sim is simulating the sensor data. It is implemented as a Kuksa Feeder, interacting with the Kuksa Data Broker only. It is supposed to be deployed as a headless container running on the device.

### Input

```
Vehicle.Chassis.Accelerator.PedalPosition : uint8 in percent
Vehicle.Chassis.Break.PedalPosition : uint8 in percent
Vehicle.Chassis.SteeringWheel.Angle : int16 in degrees (positive=left, negative=right)
Vehicle.CurrentOverallWeight : uint16 in kg
```

### Output

```
Vehicle.Acceleration.Lateral : float in m/s^s
Vehicle.Acceleration.Longitudinal : float in m/s^s
Vehicle.Acceleration.Vertical : float in m/s^s
Vehicle.Speed : float in km/h
```

