# Can Simulator

- Can Simulator (`cansim`) is a script, that executes user process with `LD_PRELOAD=libcansim.so $*`.

- `libcansim.so` hooks libc functions: `open()`, `close()`, `socket()`, `bind()`, `ioctl()`, `read()`, `write()`
and provides SocketCAN simulation for a process even if no kernel support for CAN is available.

- libc hooks are handled by SeatAdjuster Engine that waits for mocked `write()` with CanID: `SECU1_CMD_1`
  and starts generating `SECU1_STAT` CAN Frames fed to mocked `read()` calls of the process.

**NOTE:** `cansim` works only for a single process, it does not provide a common IPC so
e.g. a `cansend` and `candump` can not work together on a shared engine.

## Cansim configuration

List of environment variables to customize cansim / seat adjuster simulator

- `SAE_DEBUG`: "1" -> enable Seat Adjuster Engine (SAE) debug
- `SAE_DEBUG`: "1" -> enable Seat Adjuster Engine (SAE) verbose
- `SAE_DELAY`: milliseconds to sleep in socketcan read callback. Default 10ms for real HW.
- `SAE_LRN`  : Simulate learned state(`1=LRN`,`0=NotLRN`)
- `SAE_STOP` : "1" -> simulate real hw auto stopping at 14% and 85%

- `CANSIM_LOG`     : Log file for cansim / SAE or stderr if not set.
- `CANSIM_DEBUG`   : Enable cansim debug (hook troubleshooting only)
- `CANSIM_VERBOSE` : Enable cansim verbose (hook troubleshooting only)

**NOTE:** _SAE_XXX_ values must be updated **before**
`socket(PF_CAN, SOCK_RAW, CAN_RAW)` to have effect.
