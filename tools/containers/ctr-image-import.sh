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

# allow to skip importing image with "-n" arg
IMAGE_IMPORT=1
if [ "$1" = "-n" ]; then
	IMAGE_IMPORT=0
	shift
fi

IMG="$1"
[ -z "$IMG" ] && IMG="./image/arm64_seat-service.tar"

CONTAINER="$2"
[ -z "$CONTAINER" ] && CONTAINER="seat-service"

[ -z "$IMG_TAG" ] && IMG_TAG="docker.io/arm64/$CONTAINER:latest"

# workaround for ctr via k3s sock
CTR="sudo ctr -a /run/k3s/containerd/containerd.sock"

echo "# $CONTAINER cleanup ..."
sudo killall -9 seat_service 2>/dev/null
$CTR t kill "$CONTAINER" 2>/dev/null
$CTR c rm "$CONTAINER" 2>/dev/null

# optionally skip importing image
if [ $IMAGE_IMPORT -eq 1 ] || ! $CTR i ls | grep -q $IMG_TAG; then
	echo "# $IMG_TAG cleanup ..."
	$CTR i rm "$IMG_TAG" 2>/dev/null
	echo "# Importing $IMG_TAG from: $IMG ..."
	$CTR i import "$IMG"
fi

echo "# List ctr images:"
$CTR i ls

echo "# Creating container: $CONTAINER"

set -xe

# $CTR container create --with-ns net:/var/run/netns/vxcan --tty docker.io/arm64/$CONTAINER:latest $CONTAINER

$CTR container create --env SC_STAT=1 --tty "$IMG_TAG" "$CONTAINER" # --env CAN_WAIT=300 --env CAN=vxcan1
$CTR c ls

$CTR t start -d "$CONTAINER"
set +xe

sleep 1
$CTR t ls

CTR_PID=$($CTR t ls | grep "$CONTAINER" | awk '{print $2}')
CTR_STATE=$($CTR t ls | grep "$CONTAINER" | awk '{print $3}')

echo
if [ -n "$CTR_PID" ] && [ "$CTR_STATE" = "RUNNING" ]; then
	echo "Execute: ./can-forward.sh <can_if> $CTR_PID"
else
	echo "Failed to get container PID for can forwarding!"
	exit 1
fi
