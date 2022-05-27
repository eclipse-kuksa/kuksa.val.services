#!/bin/bash
#********************************************************************************
# Copyright (c) 2022 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License 2.0 which is available at
# http://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
#*******************************************************************************/
# shellcheck disable=SC2086
# shellcheck disable=SC2181
# shellcheck disable=SC2230

HOST_VXCAN="vxcan0"
DOCKER_VXCAN="vxcan1"

# required for k3s
export CONTAINERD_ADDRESS="/run/k3s/containerd/containerd.sock"
export CONTAINERD_NAMESPACE="default"

get_docker_pid() {
	local container="$1"
	[ -z "$(which docker)" ] && return 1
	docker inspect -f '{{ .State.Pid }}' "$container" 2>/dev/null
	return $?
}

get_ctr_pid() {
	local container="$1"
	[ -z "$(which ctr)" ] && return 1
	#ctr t ps $container | grep -v PID | head -n 1 | awk '{print $1}'
	sudo --preserve-env=CONTAINERD_ADDRESS,CONTAINERD_NAMESPACE ctr t ls | grep "$container" | awk '{print $2}'
	return $?
}

get_k3s_pid() {
	# FIXME: implement getting entrypoint pid from kubelet with k3s tools
	return 1
}

print_usage() {
	echo
	echo "Usage:  $0 {-h} {-p PID} {-c container} <hw_can>"
	echo
	echo "  hw_can          Host CAN hw interface to forward. Default: can0"
	echo "  -c container    Attemmpt to get netns PID from a running container: (docker, ctr). Default: seat_service"
	echo "  -p PID          Use provided PID for transferring vxcan interface (e.g.: docker inspect -f '{{ .State.Pid }}' container)"
	echo "  -h              Prints this message"
	echo
}

CONTAINER="seat-service"

# parse arguments
while [ $# -gt 0 ]; do
	if [ "$1" = "--pid" ] || [ "$1" = "-p" ]; then
		shift
		DOCKERPID=$1
		if ! [ "$DOCKERPID" -eq "$DOCKERPID" ]; then
			echo "Invalid Container PID: $DOCKERPID"
			print_usage
			exit 1
		fi
	elif [ "$1" = "--container" ] || [ "$1" = "-c" ]; then
		shift
		CONTAINER="$1"
		if [ -z "$CONTAINER" ]; then
			print_usage
			exit 1
		fi
	elif [ "$1" = "--help" ] || [ "$1" = "-l" ]; then
		print_usage
		exit 0
	else
		CAN="$1"
	fi
	shift
done

[ -z "$CAN" ] && CAN="can0"

# try to get "seat_service" pid via docker/ctr
[ -z "$DOCKERPID" ] && DOCKERPID=$(get_docker_pid "$CONTAINER")
[ -z "$DOCKERPID" ] && DOCKERPID=$(get_ctr_pid "$CONTAINER")
if [ -z "$DOCKERPID" ]; then
	echo "Couldn't get Container $CONTAINER PID!"
	print_usage
	exit 1
fi

# check for vxcan module
if ! lsmod | grep -q vxcan; then
	modprobe vxcan
	if [ $? -ne 0 ]; then
		echo "Can't find vxcan module!"
		echo "Try compiling from ./vxcan/install.sh"
		exit 1
	fi
fi

if ! lsmod | grep -q can_gw; then
	modprobe can-gw
	if [ $? -ne 0 ]; then
		echo "Can't find can-gw module!"
		exit 1
	fi
fi

if ip -o link | grep -q "${HOST_VXCAN}@" >/dev/null; then
	echo "Cleanup vxcan.."
	sudo ip link del "$HOST_VXCAN"
fi

echo "Creating $HOST_VXCAN <--> $DOCKER_VXCAN"
sudo ip link add "$HOST_VXCAN" type vxcan peer name "$DOCKER_VXCAN" netns "$DOCKERPID"

echo "Adding cangw rules to $CAN"
# flush previous rules (not ok for multiple socketcan containers)
sudo cangw -F
sudo cangw -A -s "$CAN" -d "$HOST_VXCAN" -e
sudo cangw -A -s "$HOST_VXCAN" -d "$CAN" -e

sudo ip link set "$HOST_VXCAN" up
sudo ip link set "$CAN" up

echo "$DOCKER_VXCAN up in ns: $DOCKERPID"
sudo nsenter -t "$DOCKERPID" -n ip link set "$DOCKER_VXCAN" up
ret=$?
if [ $ret -ne 0 ]; then
	echo
	echo "### Failed to transfer $DOCKER_VXCAN in $DOCKERPID"
	echo
fi

echo "### cangw rules:"
cangw -L || true

echo "### ip link:"
ip -c -o link | grep "link/can"

# check container ns
echo "### container link:"
sudo nsenter -t "$DOCKERPID" ip -c -o link

exit $ret
