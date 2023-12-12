# Vehicle Mock Service

## About

The vehicle mock service is a service dummy allowing to control all specified actuator- and sensor-signals via a configuration file. These configuration files are expressed in a Python-based [domain-specific language (DSL)](./doc/pydoc/mocking-dsl.md).

## Why

When developing further vehicle applications, it can be very useful to be able to mock VSS actuators or sensors by specifying configuration files and without developing a new service from scratch. This vehicle mock service provides developers with the ability to do just that. Have a look at the complete concept and design ideas [here](./doc/concept.md)

## Component diagram


```mermaid
flowchart LR
    BrokerIF(( ))
    ValIF(( ))
    ServiceIF(( ))
    id1[KUKSA.val databroker] -- sdv.databroker.v1 --- BrokerIF
    ServiceIF -- kuksa.val.v1 --- id1
    id1 -- kuksa.val.v1 --- ValIF
    id2[Vehicle Mock Service] <--> ServiceIF
    BrokerIF<--> id3[Vehicle App]
    ValIF <--> id3
```

# Running mockservice

Firstly, you will need to install all necessary Python dependencies by issuing the following command in your favorite terminal:

```bash
python3 -m pip install -r ./requirements.txt
```

You can then run the vehicle mock service with

```bash
python3 mockservice.py
```

As an alternative, you can build and execute the container from the [Dockerfile](./Dockerfile) or through the [docker-build.sh](./docker-build.sh) script.

Another option, when using VS Code, is to use the [provided VSCode task](../.vscode/tasks.json) `run-mockservice` which will set up all necessary environment variables and parameters for the service to start up properly.

## Configuration

### Vehicle Mock Service

| parameter      | default value         | Environment variable               | description                     |
|----------------|-----------------------|----------------------------------------------------------------------------------|---------------------------------|
| listen address | `"127.0.0.1:50053"`   | `MOCK_ADDR`                                                                      | Listen for rpc calls            |
| broker address | `"127.0.0.1:55555"`   | if DAPR_GRPC_PORT is set:<br>`"127.0.0.1:$DAPR_GRPC_PORT"` <br>else:<br> `VDB_ADDRESS`| The address of the KUKSA.val databroker to connect to |
| broker app id  | `"vehicledatabroker"` | `VEHICLEDATABROKER_DAPR_APP_ID`                                                  | When using DAPR, this allows to configure the id of the KUKSA.val databroker to connect to. |

Configuration options have the following priority (highest at top):
1. environment variable
2. default value

### Mocking Configuration

By default, the vehicle mock service reads the configuration for the datapoints to mock from the [`mock.py`](mock/mock.py) Python file at runtime. This file is a good starting point edit own mock configurations. 

The default configuration contains behavior for:
* `Vehicle.Speed`
* `Vehicle.Cabin.Seat.Row1.Pos1.Position`
* `Vehicle.Body.Windshield.Front.Wiping.System.Mode`
* `Vehicle.Body.Windshield.Front.Wiping.System.TargetPosition`
* `Vehicle.Body.Windshield.Front.Wiping.System.ActualPosition`

This allows the vehicle mock service to be used with the [Velocitas Seat Adjuster](https://eclipse.dev/velocitas/docs/about/use_cases/seat_adjuster/) example, as well as the initial [Vehicle App template](https://github.com/eclipse-velocitas/vehicle-app-python-template). Furthermore, it can be used to develop a wiping application for the front wipers.

### Custom mocking configuration
If the mocked datapoints are not enough, the `mock.py` in this repository can be modified and mounted into the vehicle mock service container - it will then overwrite the default mock configuration with the custom one.

The full Python mocking DSL is available [here](./doc/pydoc/mocking-dsl.md)

# Running showcase GUI

Firstly, you will need to install all necessary Python dependencies by using the following command in your favorite terminal:

```bash
# install requirements of mock service
python3 -m pip install -r ./requirements.txt
```

To run the GUI do the following in your favourtie terminal:

```bash
python3 showcase_gui/GUI.py
```

If you run it from mock directory the mock datapoints defined by mock.py get used as well:
```bash
cd mock
python3 ../showcase_gui/GUI.py
```

Depending on your Python installation, you might need to install Tk on your system (e.g. `brew install tkinter` for MacOs or `sudo apt install python3-tk` for Ubuntu).

# Generating API documentation

The [API documentation](./doc/pydoc/mocking-dsl.md) is generated from Python docs and embedded into markdown files for easy rendering on Github without external hosting. The workflow `ensure-docs-up2date` makes sure that the API docs are up to date before merging a pull request. To update the docs, run

```bash
./update-api-docs.sh
```

# Using mockservice dynamically
To use the mockservice dynamically you need to use the same process. For an example see [threaded_mock.py](examples/threaded_mock.py).

# What's not supported?
Any form of array support e.g no VSS datapoint that has DataType.*ARRAY.
If values in Kuksa Databroker are modified outside of behaviors, the changes won't be picked up by the mock service. Instead try to model another behavior that listens to ACTUATOR_TARGET/VALUE and set it like this.
