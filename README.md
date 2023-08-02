# KUKSA.VAL.services

- [KUKSA.VAL.services](#kuksavalservices)
  - [Overview](#overview)
  - [Contribution](#contribution)
  - [Build Seat Service Containers](#build-seat-service-containers)
  - [Running Seat Service / Data Broker Containers](#running-seat-service--data-broker-containers)

## Overview

This repository provides you a set of example **vehicle services** showing how to define and implement these important pieces of the **Eclipse KUKSA Vehicle Abstraction Layer (VAL)**. 
[KUKSA.val](https://github.com/eclipse/kuksa.val) is offering a *Vehicle API*, which is an abstraction of vehicle data and functions to be used by *Vehicle Apps*.
Vehicle data is provided in form of a data model, which is accessible via the [KUKSA.val Databroker](https://github.com/eclipse/kuksa.val/tree/master/kuksa_databroker).
Vehicle functions are made available by a set of so-called *vehicle services* (short: *vservice*).

You'll find a more [detailed overview here](docs/README.md).

This repository contains examples of vservices and their implementations to show, how a Vehicle API and the underlying abstraction layer could be realized.
It currently consists of
* a simple example [HVAC service](./hvac_service) written in Python and
* a more complex example [seat control service](./seat_service) written in C/C++.
* a [mock service](./mock_service/) written in Python to define execute mocking behavior defined in a Python-based DSL
  
More elaborate or completely differing implementations are target of "real world grown" projects.


## Contribution

For contribution guidelines see [CONTRIBUTING.md](CONTRIBUTING.md)

If you want to define and implement your own vehicle services, there are two guidelines/how-tos available:
* [How to implement a vservice](docs/vehicle_service_howto.md)
* [Guideline for defining a vehicle service interface](docs/interface_guideline.md)

## Build Seat Service Containers

:construction_worker_woman: :construction: **This section may be a bit outdated. So, please take care!** :construction: :construction_worker_man:

From the terminal, make the seat_service as your working directory:

``` bash
cd seat_service
```

When you are inside the seat_service directory, create binaries:

``` bash
./build-release.sh x86_64

#Use following commands for aarch64
./build-release.sh aarch64
```
Build a tar file of all binaries.
``` bash
#Replace x86_64 with aarch64 for arm64 architecture
tar -czvf bin_vservice-seat_x86_64_release.tar.gz \
    target/x86_64/release/install/ \
    target/x86_64/release/licenses/ \
    proto/
```
To build the image execute following commands from root directory as context.
``` bash
docker build -f seat_service/Dockerfile -t seat_service:<tag> .

#Use following command if buildplatform is required
DOCKER_BUILDKIT=1 docker build -f seat_service/Dockerfile -t seat_service:<tag> .
```
The image creation may take around 2 minutes.

## Running Seat Service / Data Broker Containers

To directly run the containers following commands can be used:

1. Seat Service container

   By default the container will execute the `./val_start.sh` script, that sets default environment variables for seat service.
   It needs `CAN` environment variable with special value `cansim` (to use simulated socketcan calls) or any valid can device within the container.

    ``` bash
    # executes ./va_start.sh
    docker run --rm -it -p 50051:50051/tcp seat-service
    ```

    To run any specific command in the container, just append you command (e.g. bash) at the end.

    ``` bash
    docker run --rm -it -p 50051:50051/tcp seat-service <command>
    ```


For accessing data broker from seat service container there are two ways of running the containers.

1. The simplest way to run the containers is to sacrifice the isolation a bit and run all the containers in the host's network namespace with <i>docker run --network host</i>

    ``` bash
    #By default the container will execute the ./vehicle-data-broker command as entrypoint.
    docker run --rm -it --network host -e 'RUST_LOG=info,vehicle_data_broker=debug' databroker
    ```

    ``` bash
    #By default the container will execute the ./val_start.sh command as entrypoint
    docker run --rm -it --network host seat-service
    ```

1. There is a more subtle way to share a single network namespace between multiple containers.
   So, we can start a sandbox container that will do nothing but sleep and reusing a network namespace of an this existing container:

    ``` bash
    #Run sandbox container
    docker run -d --rm --name sandbox -p 55555:55555 alpine sleep infinity
    ```

    ``` bash
    #Run databroker container
    docker run --rm -it --network container:sandbox -e HOST=0.0.0.0 -e PORT=55555 databroker
    ```

    ``` bash
    #Run seat-service container
    docker run --rm -it --network container:sandbox -e HOST=0.0.0.0 -e PORT=55555 -e PORT=50051  seat-service
    ```

1. Another option is to use `<container-name>:<port>` and bind to `0.0.0.0` inside containers

## KUKSA.val and VSS version dependency

The service examples and related tests in this repository use VSS signals. VSS signals may change over time,
and backward incompatible changes may be introduced as part of major releases.
Some of the tests in this repository relies on using latest version
of [KUKSA.val Databroker](https://github.com/eclipse/kuksa.val/pkgs/container/kuksa.val%2Fdatabroker) and
[KUKSA.val DBC Feeder](https://github.com/eclipse/kuksa.val.feeders/pkgs/container/kuksa.val.feeders%2Fdbc2val).
Some code in the repository (like [Proto](https://github.com/eclipse/kuksa.val.services/tree/main/proto) definitions)
have been copied from [KUKSA.val](https://github.com/eclipse/kuksa.val).

This means that regressions may occur when KUKSA.val or KUKSA.val Feeders are updated. The intention for other KUKSA.val
repositories is to use the latest official VSS release as default.

As of today the services are based on using VSS 3.X. As part of VSS 4.0 the instance scheme for seat positions was
changed to be based on DriverSide/Middle/PassengerSide rather than Pos1, Pos2, Pos3.
This means a refactoring would be required to update to VSS 4.0.

### Known locations where an explicit VSS or KUKSA.val version is mentioned



In [integration_test.yml](https://github.com/eclipse/kuksa.val.services/blob/main/.github/workflows/integration_test.yml)
it is hardcoded which VSS version to use when starting Databroker.

`KDB_OPT: "--vss vss_release_3.1.1.json"`

In [run-databroker.sh](https://github.com/eclipse/kuksa.val.services/blob/main/.vscode/scripts/run-databroker.sh)
it is hardcoded to use VSS 3.0.

`wget -q "https://raw.githubusercontent.com/eclipse/kuksa.val/master/data/vss-core/vss_release_3.0.json" -O "$DATABROKER_BINARY_PATH/vss.json"`

In [prerequisite_settings.json](https://github.com/eclipse/kuksa.val.services/blob/main/prerequisite_settings.json)
hardcoded versions are mentioned for KUKSA.val Databroker, KUKSA.val DBC Feeder and KUKSA.val Client.
