# Integration Tests

Integration tests can be run in develop (local) mode or in CI environment for validating VAL components in Dapr/Kubernetes environment.\
SeatService is running in simulated CAN mode `CAN="cansim"` and SeatService client is used to start seat movement.

## Integration Test overview

- `integration_test/test_feeder.py`:\
    This test covers feedercan datapoints. It checks if they are registered and also that some datapoints have changing values.\
    **NOTE:** If feedercan default config is changed, make sure those changes are reflected in feeder test.

- `integration_test/test_val_seat.py`:\
    This test covers seat service metadata and several seat move scenarios.\
    It uses an external script for asking the seat to move to desired position (using `seat_svc_client` as grpc client)

- `integration_test/broker_subscriber.py`:\
Databroker grpc subscription handler (useful for scripting)

  ```text
    Usage: ./broker_subscriber.py --addr <host:name> [ --get-meta=META | --query <QUERY> --timeout <TIMEOUT> --count <COUNT> ]

    Environment:
      'BROKER_ADDR' Default: localhost:55555
      'QUERY'       SQL datapoint query. ('*' = subscribe for all meta). Default: SELECT Vehicle.Cabin.Seat.Row1.Pos1.Position
      'COUNT'       Receive specified count of events and exit (0=inf)
      'TIMEOUT'     Abort receiving if no data comes for specified timeout in seconds (0=inf)
      'META'        Comma separated list of datapoint names to query. ('*' = all meta)
  ```

## VS Code Tasks

VS Code tasks for VAL components are defined (using similar setup as in `vehicle-app-python-template`).

To execute a task, press `F1` (or `Ctrl+Shift+P`), `"Tasks: Run Task"`, select a task to run...

List of VAL core component tasks:

- `ensure-dapr` - make sure dapr is locally installed. All dapr related tasks depend on this.
- `run-databroker` - runs databroker via dapr (building amd64 binary if missing). Depends on "ensure-dapr".
- `run-seatservice` - runs seat service via dapr (building amd64 binary if missing). Depends on "run-databroker".
- `run-feedercan` - runs feeder_can from sources. Depends on "run-databroker".

List of VAL client tasks:

- `run-databroker-cli` - runs databroker client
- `run-seat-cli` - runs seat service client. Asks user for following inputs:
  - "Seat Position": Desired seat position [0..1000]. Default: 500.
  - "Wait": client prints seat position, until desired position is reached. Default: "Wait".

Helper tasks:

- `Terminate Tasks` - Stops all running vs code tasks
- `Start VAL` - Terminates all tasks and restarts VAL components. Could be useful after rebuilding binaries.
- `Clean VAL binaries` - removes VehicleDataBroker and SeatService binaries from target install to force rebuilding. Depends on "Terminate Tasks"
- `integration-test` - Runs local integration tests in `USE_DAPR=1` mode using pytest. Depends on core VAL service tasks

**NOTE:** Tasks are defined in `.vscode/tasks.json` and mostly wrap scripts in `.vscode/scripts`, but also have extras like dependencies and terminal integration.

## Local Testing with "dapr run"

Dapr mode is executing VAL binaries with `dapr run` (using similar setup as in `vehicle-app-python-template`).
Integration tests check for `USE_DAPR=1` environment variable to support standalone dapr mode (e.g. use custom dapr proxy ports and add dapr metadata to grpc calls)

### Local setup

Either use `integration-test` vs code task, or execute the follwing commands in a terminal to install python dependencies:

```shell
cd integration_test/
pip install -r requirements.txt
pip install -r requirements-dev.txt
pip install -e .
```

And then launch pytest manually:

```shell
pytest -v . --asyncio-mode=auto
```

**NOTE:** In `USE_DAPR=1` mode, tests are using `task-seat-move.sh` script for wrapping calls through `run-seatservice-cli.sh` vs task script.

### Required VS Code tasks for local testing (dapr)

Python Integration tests depend on the following VS Code tasks:

- `ensure-dapr`
- `run-databroker`
- `run-seatservice`
- `run-feedercan`

It is possible to use VS Code `Testing` panel for debugging failed test cases and also directly running the python file in debug mode.

## Local / CI testing using Docker images

This mode is a placeholder for proper Kubernetes cluster testing.
For the moment it uses **released** or at least **tagged** images from `ghcr.io`.
To force using this mode (e.g. in CI) export `USE_DAPR=0` environment variable for pytest process

Relevant scripts:

- `integration_test/it-config` : This config defines the used images, tags, and docker specific options per val container.
- `integration_test/it-seat-move.sh`: This script is used to execute `seat_svc_client` from seat_service container to initiate seat movement for integration tests.
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
