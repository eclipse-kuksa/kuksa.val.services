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

TARGET_ARCH="$1"
[ -z "$TARGET_ARCH" ] && TARGET_ARCH="x86_64"

BUILD_DIR="$2"
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

conan install -if="$BUILD_DIR" --build=missing --profile:build=default --profile:host="${SCRIPT_DIR}/toolchains/target_${TARGET_ARCH}_Release" "$SCRIPT_DIR"

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
