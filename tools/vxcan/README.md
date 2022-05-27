# Virtual CAN Tunnel (vxcan)

More info about vxcan: <http://cateee.net/lkddb/web-lkddb/CAN_VXCAN.html>

## Building vxcan locally as module

`install-vxcan.sh` script attempts to download vxcan sources and required kernel headers / build tools.

If build successfully it also loads vxcan and can-gw modules for current kernel.

`vxcan` is required to tunnel socketcan into containers.

## aarch64 binary

At the moment it is required to copy contents of `./vxcan` directory on the target device (e.g. RPI) and execute the script to build and install `vxcan` module.
If the kernel version is fixed, vxcan.ko module can be used as binary (build it on device and reuse the binary in the base image)
