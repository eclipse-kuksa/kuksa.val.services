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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$SCRIPT_DIR/src/lib/seat_adjuster/seat_controller/"
TARGET_ARCH="x86_64"
BUILD_DIR="$SCRIPT_DIR/build_seat_controller/$TARGET_ARCH"

set -e -x

BUILD_TYPE="$1"
[ -z "$BUILD_TYPE" ] && BUILD_TYPE="Debug"

# NOTE: coverage requires Debug build...
echo "Build and install seat_controller ($BUILD_TYPE) for host architecture."

# Enable testing (on host)
CMAKE_OPT="-DSDV_BUILD_TESTING=ON -DSDV_COVERAGE=ON -DCONAN_CMAKE_SILENT_OUTPUT=ON -DCMAKE_INSTALL_PREFIX=./install -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE"

# Create Build Environment
# Use a bash shell so we can use the same syntax for environment variable
# access regardless of the host operating system
cmake -E make_directory "$BUILD_DIR"
conan install -if="$BUILD_DIR" --build=missing --profile:build=default --profile:host="${SCRIPT_DIR}/toolchains/target_${TARGET_ARCH}_${BUILD_TYPE}" "$SCRIPT_DIR"
cd "$BUILD_DIR"
# shellcheck disable=SC1091
source activate.sh # Set environment variables for cross build
# shellcheck disable=SC2086
cmake "${SOURCE_DIR}" ${CMAKE_OPT}
sleep 1
cmake --build .

# Build
echo "Test on host architecture."
make -j

# Run Unit Tests
ctest -j -T memcheck -C $BUILD_TYPE --output-on-failure

# Generate Code Coverage
make report_codecov_vservice-seat-ctrl
make report_codecov_vservice-seat-ctrl_html
make report_codecov_vservice-seat-ctrl_lcov
