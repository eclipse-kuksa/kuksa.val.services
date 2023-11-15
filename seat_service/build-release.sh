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

# Specify:
#   first argument: TARGET_ARCH = "x86_64" or "aarch64"; default: "x86_64"
#   second argument: TARGET_ARCH = "<string>; default: "$SCRIPT_DIR/target/$TARGET_ARCH/release"

# shellcheck disable=SC2086
# shellcheck disable=SC2230
# shellcheck disable=SC2046
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

print_usage() {
	echo "USAGE: $0 [OPTIONS] {TARGET}"
	echo
	echo "Compile Seat Service release binaries for target"
	echo
	echo "OPTIONS:"
	echo "  -p, --pack        Packages the binaries for building docker image in build dir"
	echo "  -d, --dir <path>  Override default release dir with specified path"
	echo "      --help        Show help"
	echo
	echo "TARGET:"
	echo "  x86_64, aarch64   Target arch to build for, default: 'x86_64'"
	echo
}

PACK=0

while [ $# -gt 0 ]; do
	if [ "$1" = "--help" ]; then
		print_usage
		exit 0
	elif [ "$1" = "-p" ] || [ "$1" = "--pack" ]; then
		PACK=1
	elif [ "$1" = "-d" ] || [ "$1" = "--dir" ]; then
		shift # advance to next arg
		BUILD_DIR="$1"
	else
		if [ "$1" != "x86_64" ] && [ "$1" != "aarch64" ]; then
			echo "Invalid Target: $1"
			print_usage
			exit 1
		fi
		TARGET_ARCH="$1"
	fi
	shift
done

# set defaults
[ -z "$TARGET_ARCH" ] && TARGET_ARCH="x86_64"
[ -z "$BUILD_DIR" ] && BUILD_DIR="$SCRIPT_DIR/target/$TARGET_ARCH/release"

# set -e
cmake -E make_directory "$BUILD_DIR"

# install last known good boost version before conan v2 mess...
### experimental stuff
export CONAN_REVISIONS_ENABLED=1

### IMPORTANT: not compatible with conan-2.0
echo "###############################"
conan --version
echo "########## Conan Info #########"
conan info .
echo "###############################"

conan install -if="$BUILD_DIR" --build=missing --profile:build=default --profile:host="$SCRIPT_DIR/toolchains/target_${TARGET_ARCH}_Release" "$SCRIPT_DIR"

cd "$BUILD_DIR" || exit
# shellcheck disable=SC1091

source ./activate.sh # Set environment variables for cross build

#CMAKE_CXX_FLAGS_RELEASE="${CMAKE_CXX_FLAGS_RELEASE} -s"
cmake -DCMAKE_BUILD_TYPE=Release -DSDV_BUILD_TESTING=OFF -DCONAN_CMAKE_SILENT_OUTPUT=ON -DCMAKE_INSTALL_PREFIX="./install" "$SCRIPT_DIR"

cmake --build . -j $(nproc)
cmake --install .
set +e

# Ensure release is sripped
if [ "$TARGET_ARCH" = "aarch64" ]; then
	STRIP="$(which aarch64-linux-gnu-strip)"
else
	STRIP="strip"
fi

echo
echo "### Check for stripped binaries"
BINARIES="./install/bin/seat_service ./install/bin/seat_svc_client ./install/bin/broker_feeder ./install/bin/tools/libcansim.so"

file $BINARIES
if [ -n "$STRIP" ]; then
	echo "### Stripping binaries in: $(pwd)"
	$STRIP -s --strip-unneeded $BINARIES
	file $BINARIES
	echo
fi

if [ "$PACK" = "1" ]; then
	ARCHIVE_PATH="target/${TARGET_ARCH}/release"
	# NOTE: Dockerfile expects target/${TARGET_ARCH}/release/* archive paths, but build dir mayt be different
	# copy everything in a temp dir to create consistent paths
	TMPDIR="/tmp/.seat_service-$$"
	[ -d "$TMPDIR" ] && rm -rf "$TMPDIR"
	mkdir -p "$TMPDIR"
	cd "$TMPDIR" || exit 1
	# create Docker compatible structure
	mkdir -p "$ARCHIVE_PATH"

	echo "# Copy all proto files in ./proto/"
	cp -ra "$SCRIPT_DIR/../proto" .
	cp -ra "$SCRIPT_DIR/proto" .
	cp -ra "$BUILD_DIR/." "$ARCHIVE_PATH"

	ARCHIVE="$SCRIPT_DIR/bin_vservice-seat_${TARGET_ARCH}_release.tar.gz"
	tar -czvf "$ARCHIVE" "$ARCHIVE_PATH/install/" "$ARCHIVE_PATH/licenses/" proto/

	# cleanup
	rm -rf "$TMPDIR"

	echo "### Packed release build to $ARCHIVE"
	ls -sh "$ARCHIVE"
fi