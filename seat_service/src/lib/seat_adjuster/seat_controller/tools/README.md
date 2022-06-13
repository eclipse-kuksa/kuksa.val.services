# Tools and helpers for SeatAdjuster Use-Case

## Scripts for SeatAdjuster HW

### `ecu-reset`

After SeatAdjuster ECU was reset (e.g. after power cycle) it loses its **Learned State** and special Normalization procedure needs to be executed.

This scrpt handles normalization procedure (if needed):

    Usage: ./ecu-reset {-h} {-s} {-v} {-f} {-t timeout_sec} {can_if}
        can_if: CAN interface to use. Default: can0
        -s: Prints RX Can frames (Useful for troubleshooting)
        -t: timeout in seconds to abort operation. Default: 60 sec
        -f: Force calibration even if motor reports learned state
        -h: Prints this message

#### Preconditions for running `ecu-reset`

- After Seat ECU power off / on.
- `SECU1_STAT` shows motor learning state is `Not Learned`.
- `SECU1_STAT` shows _Invalid Position_ (`255`) for motor1_pos.
- Some drift is noticed in threshold values (auto stop at ~14% and 85%). May need to reset the range. In this case run with `-f` argument.

**NOTE:**
`ecu-reset` script does not perform calibration if incoming frames show motor is in
**Learned state**.

It is advised to include a call to `ecu-reset` just before starting grpc service,
so it starts in calibrated state.
But it is also possible that grpc service detects unlearned state because of ECU power cycle

### `motor-dec`

Sends `SECU1_CMD_1` command to SeatAdjuster ECU for manual move (decreasing position) and specified optional RPM.

    Usage: ./motor-inc <rpm>
        rpm - SECU1_CMD_1.motor1_rpm value [0..254]. Actual RPM are <rpm>*100. Default: 80

### `motor-inc`

Sends `SECU1_CMD_1` command to SeatAdjuster ECU for manual move (increasing position) and specified optional RPM.

    Usage: ./motor-dec <rpm>
        rpm - SECU1_CMD_1.motor1_rpm value [0..254]. Actual RPM are <rpm>*100. Default: 80

### `motor-stop`

Sends `SECU1_CMD_1` command to SeatAdjuster ECU for stopping all motors.

### `motor-swipe`

Runs the motors in endless increasing / decreasing position loop with specified movement timeout.

    Usage: ./motor-swipe {can_if} {timeout}
        can_if - use specified can interface. Default: can0
        timeout - time for running seat motor inc/dec direction. Default: 5 sec.

### Seat Adjuster ECU limitations

- There is a certain threshold for aborting motor **decreasing** command at ~20%. It is possible to move at 0% with additional `SECU1_CMD_1` command (after motors had stopped)
- There is a certain threshold for aborting motor **increasing** command at ~80%. It is possible to move at 100% with additional `SECU1_CMD_1` command (after motors had stopped)
- _rpm_ values below 30 (3000 rpm) are too low to start moving.
- _rpm_ values above 120 (12000 rpm) no longer increase the speed of movement.


## Scripts for testing without CAN HW

### `setup-vcan`

Setup `vcan0` on host (if interface does not exist).

Both `grpc` server and `sim-SECU1_STAT` should run on `vcan0`.

### `sim-SECU1_STAT`

  Simulate `SECU1_STAT.motor1_pos` changing from _[0..100]_ and _[100..0]_

  This script is required if you don't have CAN hardware for Seat Adjuster as Control Loop requires a defined initial `motor1_pos` to initiate the move command.

  **NOTE:** It is recommended to run both `hal_service` and `sim-SECU1_STAT` on `vcan0`.

    Usage: ./sim-SECU1_STAT <can_if> {iterations} {verbose}
        can_if - use specified can interface. Default: vcan0
        iterations - runs pos change [0..100]-[100..0] in a loop. 0=infinite, Default: 0
        verbose - if 3rd arg is provided - prints generated cansend commands


## CAN Troubleshooting scripts

### `cangen-SECU1_STAT`

Generate **random** can frame payload for `SECU1_STAT` CanID.
Can be used to stress-test with invalid data.

    Usage: ./cangen-SECU1_STAT <can_if> {timeout}
        can_if - use specified can interface. Default: can0
        timeout - delay between random frames. default 1000ms.
