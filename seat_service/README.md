# Seat Service Example

- [Seat Service Example](#seat-service-example)
  - [Overview](#overview)
    - [Context](#context)
    - [Internals](#internals)
  - [Development environment](#development-environment)
    - [Prerequisites](#prerequisites)
    - [Usage on CLI](#usage-on-cli)
      - [Build Seat Service](#build-seat-service)
    - [Usage in Visual Studio Code](#usage-in-visual-studio-code)
  - [Configuration](#configuration)
  - [Seat Controller Documentation](#seat-controller-documentation)
  - [Generate documentation](#generate-documentation)

## Overview

This represents the example seat control service. More elaborate or completely differing implementations are target of particular projects providing a vehicle abstraction layer.
### Context
![SeatService_context](docs/assets/SeatService_context.svg)

### Internals
![SeatService_internal](docs/assets/SeatService_internal.svg)

## Development environment

### Prerequisites

1. Install and configure (if needed) local authentication proxy e.g. CNTLM or Px
2. Install and configure docker
   - [Get Docker](https://docs.docker.com/get-docker/)
3. Build base development docker Image

   ``` bash
   cd tools/ && \
   docker build -t oci_kuksa-val-services-ci:latest .
   ```

### Usage on CLI

From the checked-out git folder, to enter a shell execute:

``` bash
//Linux
docker run --rm -it -v $(pwd):/workspace oci_kuksa-val-services-ci:latest <build-command>

//Windows (cmd)
docker run --rm -it -v %cd%:/workspace oci_kuksa-val-services-ci:latest <build-command>

//Windows (Powershell)
docker run --rm -it -v ${PWD}:/workspace oci_kuksa-val-services-ci:latest <build-command>
```

#### Build Seat Service

Building the seat service via dev container must be triggered from the project root folder (seat service is referencing vehicle_data_broker/proto), e.g.:

``` bash
//Linux
docker run --rm -it -v $(pwd):/workspace oci_kuksa-val-services-ci:latest /bin/bash -c "cd seat_service; ./build-debug.sh"
```

### Usage in Visual Studio Code

It is also possible to open the repo as a remote container in VScode using the approach [Developing inside a Container](https://code.visualstudio.com/docs/remote/containers). All needed tools for VScode are automatically installed in this case

1. Install VScode extension with ID  ```ms-vscode-remote.remote-containers```
2. Hit "F1" and type ``Remote-Containers: Reopen in Container``

## Configuration

| parameter      | default value | cli parameter  | Env var                       | description                     |
|----------------|---------------|----------------|-------------------------------|---------------------------------|
| can_if_name    | -             | can_if_name    | -                             | Use socketCAN device            |
| listen_address | "localhost"   | listen_address | -                             | Listen for rpc calls            |
| listen_port    | 50051         | port           | -                             | Listen for rpc calls            |
| broker_address | "localhost"   | -              | -                             | Connect to data broker instance |
| broker_port    | 55555         | -              | DAPR_GRPC_PORT                | Connect to data broker instance |
| broker_app_id  | <deactivated> | -              | VEHICLEDATABROKER_DAPR_APP_ID | Connect to data broker instance |

Further configuration of the seat controller see [Seat Controller Documentation](#seat-controller-documentation).

## Seat Controller Documentation

Seat Controller module handles SocketCAN messaging and provides Control Loop for moving a seat to desired position.
It also provides `cansim` module for simulating a HW Seat ECU even without `vcan` support (e.g. CI pipeline).

For more details about Seat Controller, Seat CAN Simulator and related tools,
check [README](./src/lib/seat_adjuster/seat_controller/README.md)

## Generate documentation

- Run Doxygen:
  doxygen is able to run with the following command from the main directory:

  ``` bash
    doxygen ./docs/doxygen/doxyfile
  ```

  or using:

    ``` bash
    build-docu.sh
  ```

- The output will be stored to ``./docs/out``. You can watch the documentation with open the following file in the browser:

  ``./docs/doxygen/out/html/index.html``
