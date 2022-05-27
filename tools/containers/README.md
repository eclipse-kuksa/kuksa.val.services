# SocketCAN tunnelling from target device to container

## Overview

Since the vehicle services need access to the CAN bus, we need to tunnel `SocketCAN` from actual host physical can interface to container(s).
Where in a professional edge stack `SocketCAN` might not be the right choice to use.

Several options are possible

### Use host-networking for container

It is possible to share host networking with seat-service container and directly see all host can interfaces from the container, although that solution has significant security impact.

**Pros:**

* if container code is trusted, this is the easiest solution for accessing 'SocketCAN' within a container.

**Cons:**

* all Network interfaces and all open port on the host can be seen in the container.
* modify interfaces where no root access is needed

### Setup vcan inside the container and use can 2 UDP bridge

For creating a `vcan` interface inside the container, it needs to be started with: `--cap-add=NET_ADMIN`
and host must have vxcan module installed: `sudo modprobe vcan`

For actual can tunnelling e.g. build [cannelloni](https://github.com/mguentner/cannelloni) for host and into container.
Then use standalone `cannelloni` instance to communicate on a pre-defined server/client UDP ports (single client only!).

**NOTE:** This does not work in CI pipeline (no vcan module available)

### Use vxcan / cangw socketcan tunnelling to containers

Initial idea taken from [here](https://www.lagerdata.com/articles/forwarding-can-bus-traffic-to-a-docker-container-using-vxcan-on-raspberry-pi)
and [here](https://chemnitzer.linux-tage.de/2021/de/programm/beitrag/210)

`vxcan0` interface is created on host and it is tunnelling to `vxcan1` (later moved into container's namespace)
`cangw` rules are defined to map everything (reads+writes) from hw `can` interface to `vxcan0`. It is possible to define filters so some specific canIDs are forwarded.

It has some additional requirements:

* `vxcan` module needs to be build for a specific kernel. Either as separate module or as par of the Kernel. Check [here](../vxcan/README.md).
* `can-gw` module is needed on host.
* `./can-forward.sh` script must be executed on host (requires root), just after starting the container to set up `vxcan`.
* container is started without `vxcan` tunnel initially and should wait for `./can-forward.sh` script on host to transfer `vxcan` into container's namespace.
  `CAN_WAIT=30` and `CAN=vxcan1` environment variables are set by default for the container, so it's entrypoint waits for 30s until `vxcan1` interface is available.
* `./can-forward.sh` needs the PID of the container's entry point, it has some auto-detection for docker, ctr but probably needs tweaking for k3s.

**NOTE:** This option does not work on CI for obvious reasons.

**Pros:**

* container can have customized (`cangw` rule based) access to certain 'SocketCAN' messages in its own namespace and does not see host Network interfaces.
* no privileged access is required for the container.

**Cons:**

* needs additional post-configuration step(s) (e.g.: execute `can-forward.sh`) for k3/8s.

**Hint** Possible solutions:

* [Init Containers | Kubernetes](https://kubernetes.io/docs/concepts/workloads/pods/init-containers/) could be used, but must run with elevated rights -> problematic since the steps need to run after container startup.
* listen to k8s lifecycle events.
* Containers which need CAN access need to "tagged".
* Additional requirement to edge_containerd or k8s control plain
  * get pid of started container.
  * configure network setup.

### Use cansim within the container if hw socketcan is not required

`cansim` option for container is also available. It allows running a binary with mocked socketcan related libc calls, so will work without socketcan requirements in the container.

To use cansim you can define `CAN=cansim` in the container's environment, it overrides `CAN_WAIT` and container does not need to wait for an interface.

**NOTE:** `CAN=cansim` is set by default in seat-service container.

## Helper Scripts

### `can-forward.sh`

Helper script for tunnelling host socketcan to a container using vxcan kernel driver and cangw (needs root!).

    Usage: ./can-forward.sh {-h} {-p PID} {-c container} <hw_can>

    hw_can          Host CAN hw interface to forward. Default: can0
    -c container    Attemmpt to get netns PID from a running container: (docker, ctr). Default: seat-service
    -p PID          Use provided PID for transferring vxcan interface (e.g.: docker inspect -f '{{ .State.Pid }}' container)
    -h              Prints this message

### `ctr-image-import.sh`

Helper script for importing container OCI image using `ctr`.

    Usage: ./ctr-image-import.sh [ OCI_IMAGE [ CONTAINER ] ]

    Arguments:
      OCI_IMAGE  tar file with OCI image. Default: ./image/arm64_seat-service.tar
      CONTAINER  Container name. Default: seat-service
