# Seat Controller

Seat Controller module handles low-level `SocketCAN` control loop for moving a seat.
It updates current positions listening for `SECU1_STAT` Signal (CanID:`0x712`, motor1).

When new seat position is requested, control loop checks it against current seat position
and decides direction in which to move the seat using `SECU1_CMD1` signal (CanID:`0x705`, motor1).
After reaching desired position, control loop stops seat movement.

It is possible to have a small (~1%) "overshoot" in position.

## Seat Controller Configuration

To maximize flexibility SeatController uses environment variables to easily override default configuration.

- `SC_CAN`: If set, overrides requested can interface to use.
- `SC_TIMEOUT`: Seat adjuster move operation timeout (ms). After it is reached motors are stopped.
- `SC_STAT`: "1" = dump SECU1_STAT can frames (useful to check for unmanaged seat position changes)
- `SC_CTL`: "1" = dump Coontrol Loop messages. Only dumps when active set position operation is running.
- `SC_RPM`: Seat moror `RPMs / 100`. e.g. `80=8000rpm`. Suggested range `[30..100]`
- `SC_RAW`: "1" = enables raw can dumps, too verbose (only for troubleshooting).
- `SC_VERBOSE`: "1" = enables verbose dumps (only for troubleshooting).

### Guarded Seat Adjuster external checks

Guarded Seat Adjuster enables checking for certain conditions e.g. is it safe to move a seat
and denies access to SeatController module if system is not in a _safe_ state.

SeatAdjuster C++ module uses the following environment variables:

- `SA_DEBUG`: "1" = enable SeatAdjuster C++ debug.
- `SA_EXIT`: "1" = Rxit GRPC service in case of CAN errors / or other fatal errors.

## Seat Controller Tools

For more details check tools [README](./tools/README.md)

## Seat ECU / CAN simulator

It is possible to run `grpc` service without Seat ECU or even without SocketCAN support (e.g. in github action)

For more details check cansim [README](./tests/cansim/README.md)
