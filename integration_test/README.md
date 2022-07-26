# Integration Tests

Integration tests can be run in develop (local) mode for Dapr validation or with VAL docker containers (also running on CI).

- Integration tests in Local mode are using VS Code tasks defined in `.vscode/tasks.json`, see [README there](../.vscode/README.md).
- Container mode uses released containers for databroker and dbc2val and can also build seat and hvac service containers to test with unreleased changes.

## Integration test overview

Integration tests `./integration_test/test_*.py` are based on pytest and use external bash scripts (wrapping service clients) to communicate with respective services.

### Seat Service integration test

`integration_test/test_val_seat.py` - This test covers seat service metadata and several seat move scenarios.

SeatService is tested in simulated CAN mode `CAN="cansim"`. It uses an external script for asking the seat to move to desired positions (0, 1000, invalid),
using `seat_svc_client` as grpc client.

### HVAC Service integration test

`integration_test/test_hvac.py` - This test checks if Hvac Service datapoints (`Vehicle.Cabin.DesiredAmbientAirTemperature`,
`Vehicle.Cabin.IsAirConditioningActive`) are registered in Kuksa Data Broker and and after invoking external script to call
HVAC client, checks if expected values are fed to Data Broker.

### CAN Feeder (dbc2val) integration test

`integration_test/test_feeder.py` - dbc2val is also included for integration testing as we don't have a master project to test all components.

This test covers several dbc2val datapoints (`Vehicle.OBD.Speed`, `Vehicle.OBD.EngineLoad`).

It checks if they are registered and also that some datapoints have changing values.

**NOTE:** dbc2val default config must be changed as it feeds to `kuksa.val` server by default.\
Custom configuration files are mapped to container as volume from: `./integration_test/volumes/dbc2val` and
environment variables (`FEEDERCAN_*`) are set in `./it-config` script and also in vscode task: `run-feedercan.sh`.\
Make sure `test_feeder.py` datapoints are available (and changing each second) in `./volumes/dbc2val/it-candump0.log`.

### Data Broker subscription helper

`.integration_test/broker_subscriber.py` - Helper for Data Broker grpc subscriptions. Not a testcase, but useful for command line scripting and checking events without Data Broker client.

```text
  Usage: ./broker_subscriber.py --addr <host:name> [ --get-meta=META | --query <QUERY> --timeout <TIMEOUT> --count <COUNT> ]

  Environment:
    'BROKER_ADDR' Default: localhost:55555
    'QUERY'       SQL datapoint query. ('*' = subscribe for all meta). Default: SELECT Vehicle.Cabin.Seat.Row1.Pos1.Position
    'COUNT'       Receive specified count of events and exit (0=inf)
    'TIMEOUT'     Abort receiving if no data comes for specified timeout in seconds (0=inf)
    'META'        Comma separated list of datapoint names to query. ('*' = all meta)
```

## Local Testing with "dapr run"

Dapr mode is executing VAL binaries with `dapr run` (using similar setup as in [vehicle-app-python-template](https://github.com/eclipse-velocitas/vehicle-app-python-template)).

Integration tests check for `USE_DAPR=1` environment variable to support standalone dapr mode (e.g.
use custom dapr proxy ports and add dapr metadata to grpc calls), it also affects the script used to invoke different clients.

### Local (dapr mode) setup

If needed, change external VAL component versions in `./prerequisite_settings.json` (also used from vs code tasks to download assets).

For running tests either use `integration-test(local)` vs code task, or execute the follwing commands in a terminal to install python dependencies:

```shell
cd integration_test/
pip install -r requirements.txt
pip install -r requirements-dev.txt
pip install -e .

USE_DAPR=1 pytest -v . --asyncio-mode=auto
```

**NOTE:** In `USE_DAPR=1` mode, tests are using `.integration_test/task-*.sh` scripts for wrapping calls through existing vs code task scripts.

### Required VS Code tasks for local testing (dapr)

Python Integration tests depend on the following VS Code tasks:

- `ensure-dapr`
- `run-databroker`
- `run-seatservice`
- `run-hvacservice`
- `run-feedercan`

It is possible to use VS Code `Testing` panel for debugging failed test cases and also directly running the python file in debug mode.

## Local / CI testing with Docker images

This mode is a placeholder for proper Kubernetes cluster testing. It uses released/tagged images from `ghcr.io`.

To force using this mode (e.g. in CI) export `USE_DAPR=0` environment variable for pytest process or simply use vs code task `intergarion-test (containers)`.

Relevant scripts:

- `integration_test/it-config` : This config defines the used images, tags, and docker specific options per val container.\
  **NOTE**: It gets Data Broker and dbc2val tags from `./prerequisite_settings.json`, and if `SEAT_TAG`, `HVAC_TAG` environment variables are set to `prerelease`,
  seat_service and hvac_service are build from local sources and tagged as ghcr.io images.
- `integration_test/it-seat-move.sh`: This script is used to execute `seat_svc_client` from seat_service container to initiate seat movement for integration tests.
- `integration_test/it-hvac-cli.sh`: This script is used to execute `./hvac_service/testclient.py` to request hvac temperature and status changes.
- `integration_test/it-setup.sh`: This is the main script handling val containers lifecycle:

    ```text
    Usage:  ./it-setup.sh {Options} [ init | start | stop | status | cleanup ]

    Options:
      --force  for 'init' and 'cleanup' commands, forces rebuilding/pulling/removing VAL images
      --logs   for 'status' command, shows docker logs per var container
      --help   Prints this message and exit.

    Commands:
      init     Pulls VAL images from a repository or builds them if missing (use --force to force init)
      start    Starts VAL Containers (also implies init)
      stop     Stops VAL Containers
      status   Shows status of VAL Containers. Use --log to see last logs from VAL containers
      cleanup  Removes VAL Containers. Use --force to also remove configured VAL images
    ```
