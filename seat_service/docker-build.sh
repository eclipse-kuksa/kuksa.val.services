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
# shellcheck disable=SC2181
# shellcheck disable=SC2086
# shellcheck disable=SC2230

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASEDIR="$SCRIPT_DIR/.."

print_usage() {
	echo "USAGE: $0 [OPTIONS] TARGETS"
	echo
	echo "Standalone build helper for seat-service container."
	echo
	echo "OPTIONS:"
	echo "  -l, --local      local docker import (does not export tar)"
	echo "      --help       show help"
	echo
	echo "TARGETS:"
	echo "  x86_64, aarch64  Target arch to build for, if not set - defaults to multiarch"
	echo
}

LOCAL=0
while [ $# -gt 0 ]; do
	if [ "$1" = "--local" ] || [ "$1" = "-l" ]; then
		LOCAL=1
	elif [ "$1" = "--help" ]; then
		print_usage
		exit 0
	else
		TARGET="$1"
		break
	fi
	shift
done

target_arch() {
	local target="$1"
	case "$target" in
	"x86_64")
		echo "amd64"
		;;
	"aarch64")
		echo "arm64"
		;;
	"")
		echo "multiarch"
		;;
	*)
		return 1
		;;
	esac
	return 0
}

build_release() {
	local arch="$1"

	cd "$BASEDIR/seat_service" || return 1
	echo "-- Building release for: $arch ..."
	./build-release.sh "$arch"

	echo "-- Building bin_vservice-seat_${arch}_release.tar.gz ..."

	tar -czvf "bin_vservice-seat_${arch}_release.tar.gz" \
		"target/${arch}/release/install/" \
		"target/${arch}/release/licenses/" \
		"proto/"
}

# Dockerfile requires both bin_vservice-seat_*_release artifacts
build_release aarch64
build_release x86_64

cd "$BASEDIR" || exit 1
echo "-- Building seat-service container ..."
# DOCKER_BUILDKIT=1 docker build -f seat_service/Dockerfile -t seat-service .

if [ -z "$TARGET" ] && [ $LOCAL -eq 1 ]; then
	echo "Multiarch archives are not supported for local builds, removing --local flag ..."
	LOCAL=0
fi

DOCKER_ARCH=$(target_arch "$TARGET")
DOCKER_EXPORT="./${DOCKER_ARCH}_seat-service.tar"

if [ "$DOCKER_ARCH" = "multiarch" ]; then
	DOCKER_ARGS="--platform linux/amd64,linux/arm64 -t $DOCKER_ARCH/seat-service --output type=oci,dest=$DOCKER_EXPORT"
else
	if [ $LOCAL -eq 1 ]; then
		DOCKER_ARGS="--load -t $DOCKER_ARCH/seat-service"
		DOCKER_EXPORT="(local)"
	else
		DOCKER_ARGS="--platform linux/$DOCKER_ARCH -t $DOCKER_ARCH/seat-service --output type=oci,dest=$DOCKER_EXPORT"
	fi
fi

echo "# docker buildx build $DOCKER_ARGS -f seat_service/Dockerfile ."
DOCKER_BUILDKIT=1 docker buildx build $DOCKER_ARGS -f seat_service/Dockerfile .
[ $? -eq 0 ] && echo "# Exported $DOCKER_ARCH/seat-service in $DOCKER_EXPORT"
