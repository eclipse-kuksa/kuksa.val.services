# Driver simulator

This kuksa service simulates a driver that switches between good and bad drving modes. 

- While in the "good" driving mode it accelerates slowly until it reaches the maximal possible acceleration, then let's off the throttle,
drives at constant speed for a while and start braking slowly, with the brake action peaking and then letting off until the car is stopped. During that it also changes the steering angle slowly from 0 to 20 and back down to 0.

- While in the "bad" driving mode it presses the throttle to the max for a while, then brakes with maximum brake after that. This cycle repeats 4 times. During the bad driving cycle it also steers randomly between -5 and 5 degrees.


1) run kuksa with `docker run -e RUST_LOG="debug" --rm -it -p 55555:55555/tcp ghcr.io/eclipse/kuksa.val/databroker:master`
2) pip3 install -r requirements.txt
3) python3 driver_runner.py

(or run the pre-built container in the registry)