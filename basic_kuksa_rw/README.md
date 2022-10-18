# To run minimal kuksa read/write example

1) docker run -e RUST_LOG="debug" --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master

2) pip3 install -r requirements.txt

3) python3 test_rw.py


# To run car simulator 
1) run kuksa with `docker run -e RUST_LOG="debug" --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master`
2) pip3 install -r requirements.txt
3) python3 bicycle_runner.py

The service will now listen for changes to:
- "Vehicle.Chassis.Brake.PedalPosition"
- "Vehicle.Chassis.Accelerator.PedalPosition"
- "Vehicle.Chassis.SteeringWheel.Angle"

And publish updates for while running the simulation:

- "Vehicle.Speed"
- "Vehicle.Acceleration.Lateral"
- "Vehicle.Acceleration.Longitudinal"
- "Vehicle.Acceleration.Vertical"

Use the environmental variable `SIM_SPEED` (default=0.001 /seconds/) to change simulation update speed
And `LOG_LEVEL` to set the logging level for the simulator runner.

# To run simple driver simulator:

With the car simulator service already running, in a seperate terminal run:

```python
python3 simple_driver.py
```

This driver uses the differnece of two gaussians with different mean values to generate a smooth control curve
With control > 0 meaning acceleration and control < 0 meaning breaking. This driver app also only goes in a straight line
(does not use the steering controls).

$$
control(time)= \frac{1}{0.5\sqrt{\left(2\pi\right)}}\exp\left(-\left(\frac{time-2}{0.5}\right)^{2}\right)-\frac{3}{0.7\sqrt{\left(2\pi\right)}}\exp\left(-\left(\frac{time-15}{0.7}\right)^{2}\right)
$$


