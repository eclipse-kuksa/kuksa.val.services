# Seat Service Example

- [Seat Service Example](#seat-service-example)
  - [Overview](#overview)
    - [Context](#context)
    - [Internals](#internals)
  - [Development environment](#development-environment)
    - [Building/testing in Github codespaces](#buildingtesting-in-github-codespaces)
    - [Local development](#local-development)
      - [Prerequisites](#prerequisites)
      - [Usage on CLI](#usage-on-cli)
        - [Building on Ubuntu 20.04](#building-on-ubuntu-2004)
      - [Building in DevContainer](#building-in-devcontainer)
        - [Build Seat Service binaries](#build-seat-service-binaries)
        - [Build Seat Service container](#build-seat-service-container)
    - [Usage in Visual Studio Code](#usage-in-visual-studio-code)
  - [Configuration](#configuration)
    - [Command line arguments](#command-line-arguments)
    - [Environment variables](#environment-variables)
    - [Entrypoint script variables](#entrypoint-script-variables)
    - [Seat Controller configuration](#seat-controller-configuration)
  - [Seat Controller Documentation](#seat-controller-documentation)
  - [Generate documentation](#generate-documentation)

## Overview

This represents the example seat control service. More elaborate or completely differing implementations are target of particular projects providing a vehicle abstraction layer.

### Context

![SeatService_context](docs/assets/SeatService_context.svg)

### Internals

![SeatService_internal](docs/assets/SeatService_internal.svg)

## Development environment

### Building/testing in Github codespaces

For `aarch64` hosts or for quickly testing a pull request it is easier to use [Github codespaces](https://docs.github.com/en/codespaces/overview).

- Create a codespace from [here](https://github.com/eclipse/kuksa.val.services/codespaces)
- Choose: `Configure and create codespace`
- Choose a branch to run on. If you just see `main` and need to test a PR, you can select `Files changed` in the PR, then `Review in codespace`
- Wait several minutes for building codespace devcontainer.
- If everything is OK, you'll see several `vscode` tasks running sequentially (e.g `run-<component>`)
- `run-seatservice` will build the Seat Service from sources using [build-release.sh](./build-release.sh)
- If you need to start another `vscode` task / or stop them all, use `F1`, `Tasks: Run Task`, select desired task
- There are 2 `integration-test` tasks for testing local versions or released containers

### Local development

#### Prerequisites

1. Install and configure (if needed) local authentication proxy e.g. CNTLM or Px
1. Install and configure docker: [Get Docker](https://docs.docker.com/get-docker/)
1. Build base development docker. Go to the top-level of the repo

    ``` bash
    cd ..
    docker build -t oci_kuksa-val-services-ci:latest -f tools/Dockerfile .
    # NOTE: If you need to cross compile for different arch:
    DOCKER_BUILDKIT=1 docker buildx build --platform linux/arm64 -t oci_kuksa-val-services-ci:arm64 -f tools/Dockerfile --load .
    ```

#### Usage on CLI

**NOTE:** Building Seat Service on `aarch64` host is not supported at the moment.

##### Building on Ubuntu 20.04

You can use dedicated build docker script [docker-build.sh](./docker-build.sh) if host environment matches target (Ubuntu 20.04):

``` bash
# Linux: [Ubuntu 20.04]
./seat_service/docker-build.sh

USAGE: ./docker-build.sh [OPTIONS] TARGETS
Standalone build helper for seat_service docker image.

OPTIONS:
  -l, --local      local docker import (does not export tar)
  -v, --verbose    enable plain docker output and disable cache
      --help       show help

TARGETS:
  x86_64|amd64, aarch64|amd64    Target arch to build for, if not set - defaults to multiarch
```

#### Building in DevContainer

If you are using different distro / version, you may use the devcontainer to compile seat service binaries.

From the checked-out git folder, to enter a shell execute:

``` bash
# Linux: [x86_64, any version]
docker run --rm -it -v $(pwd):/workspace oci_kuksa-val-services-ci:latest <build-command>

# Windows (cmd)
docker run --rm -it -v %cd%:/workspace oci_kuksa-val-services-ci:latest <build-command>

# Windows (Powershell)
docker run --rm -it -v ${PWD}:/workspace oci_kuksa-val-services-ci:latest <build-command>
```

##### Build Seat Service binaries

Building the seat service via dev container must be triggered from the project root folder (seat service is referencing kuksa_data_broker/proto), e.g.:

``` bash
# Linux

# Cleanup any build artifacts
rm -rf seat_service/bin_vservice-seat_*.tar.gz  seat_service/target/

# Generate seat_service/bin_vservice-seat_*.tar.gz files for packing seat service container
docker run --rm -it -v $(pwd):/workspace oci_kuksa-val-services-ci:latest /bin/bash -c \
  "cd seat_service/; ./build-release.sh"
```

##### Build Seat Service container

Build the container using pre-built binaries: `seat_service/bin_vservice-seat_*.tar.gz`

``` bash
# Linux
docker build -t seat_service -f seat_service/Dockerfile .
```

### Usage in Visual Studio Code

It is also possible to open the repo as a remote container in VScode using the approach [Developing inside a Container](https://code.visualstudio.com/docs/remote/containers).
All needed tools for VScode are automatically installed in this case

1. Install VScode extension with ID  ```ms-vscode-remote.remote-containers```
1. Hit `F1` and type `Remote-Containers: Reopen in Container`

## Configuration

### Command line arguments

``` console
./seat_service can_if_name [listen_address [listen_port]]
```

| cli parameter  | default value     | description                    |
|----------------|-------------------|--------------------------------|
| can_if_name    | -                 | Use socketCAN device           |
| listen_address | `"localhost"`     | Listen address for grpc calls  |
| listen_port    | `50051`           | Listen port for grpc calls     |

### Environment variables

| Environment variable            | default value         | description                       |
|---------------------------------|-----------------------|-----------------------------------|
| `BROKER_ADDR`                   | `"localhost:55555"`   | Connect to databroker `host:port` |
| `VSS`                           | `4`                   | VSS compatibility mode [`3`, `4`] |
| `DAPR_GRPC_PORT`                | `55555`               | Dapr mode: override databroker port replacing `port` value in `$BROKER_ADDR` |
| `VEHICLEDATABROKER_DAPR_APP_ID` | `"vehicledatabroker"` | Dapr app id for databroker        |
| `SEAT_DEBUG`                    | `1`                   | Seat Service debug: 0=ERR, 1=INFO, ...     |
| `DBF_DEBUG`                     | `1`                   | DatabrokerFeeder debug: 0=ERR, 1=INFO, ... |

### Entrypoint script variables

There is dedicated entry point script [val_start.sh](./src/lib/seat_adjuster/seat_controller/tools/val_start.sh)
that runs seat service with additional Environment variable configuration:

| Environment variable            | default value         | description                             |
|---------------------------------|-----------------------|-----------------------------------------|
| `CAN`                           | `"can0"`              | Overrides `can_if_name` cli argument    |
| `SERVICE_HOST`                  | `"0.0.0.0"`           | Overrides `listen_address` cli argument |
| `SERVICE_PORT`                  | `50051`               | Overrides `listen_port` cli argument    |
| `SC_RESET`                      | -                     | If != "0", executes `ecu-reset` script to calibrate seat motors |

**NOTE:** Check `val_start.sh` script comments for less-important Environment variables.

### Seat Controller configuration

Further configuration of the seat controller see [Seat Controller Documentation](#seat-controller-documentation).

## Seat Controller Documentation

Seat Controller module handles SocketCAN messaging and provides Control Loop for moving a seat to desired position.
It also provides `cansim` module for simulating a HW Seat ECU even without `vcan` support (e.g. CI pipeline).

For more details about Seat Controller, Seat CAN Simulator and related tools,
check [SeatController README](./src/lib/seat_adjuster/seat_controller/README.md)

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
  `./docs/doxygen/out/html/index.html`
