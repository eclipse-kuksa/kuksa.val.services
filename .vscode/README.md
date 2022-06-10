# VS Code Tasks

VS Code tasks are used to execute the VAL components (using similar setup as in `vehicle-app-python-template`).

To execute a task, press `F1` (or `Ctrl+Shift+P`), `"Tasks: Run Task"`, select a task to run...

List of VAL core component tasks:

- `ensure-dapr` - make sure dapr is locally installed. All dapr related tasks depend on this.
- `run-databroker` - runs databroker via dapr (building amd64 binary if missing). Depends on "ensure-dapr".
- `run-hvacservice` - runs the HVAC service via dapr. Depends on "run-databroker".
- `run-seatservice` - runs seat service via dapr (building amd64 binary if missing). Depends on "run-databroker".

List of VAL client tasks:

- `run-databroker-cli` - runs databroker command line interface client
- `run-seat-cli` - runs seat service client. Asks user for following inputs:
  - "Seat Position": Desired seat position [0..1000]. Default: 500.
  - "Wait": client prints seat position, until desired position is reached. Default: "Wait".
- `run-hvac-cli` - runs the HVAC test client allowing to enable/disable the AC and setting the desired cabin temperature.

Helper tasks:

- `Terminate Tasks` - Stops all running vs code tasks
- `Start VAL` - Terminates all tasks and restarts VAL components. Could be useful after rebuilding binaries.
- `Clean VAL binaries` - removes VehicleDataBroker and SeatService binaries from target install to force rebuilding. Depends on "Terminate Tasks"
- `integration-test` - Runs local integration tests in `USE_DAPR=1` mode using pytest. Depends on core VAL service tasks

**NOTE:** Tasks are defined in `.vscode/tasks.json` and mostly wrap scripts in `.vscode/scripts`, but also have extras like dependencies and terminal integration.
