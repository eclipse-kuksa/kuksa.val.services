# To run car simulator 
1) run kuksa with `docker run--rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master`
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

Use the environmental variable `SIM_SPEED` (default=2 /seconds/) to change simulation update speed
And `LOG_LEVEL` to set the logging level for the simulator runner.



