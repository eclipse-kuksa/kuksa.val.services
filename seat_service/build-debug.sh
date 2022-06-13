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
#   second argument: TARGET_ARCH = "<string>; default: "$SCRIPT_DIR/target/$TARGET_ARCH/Debug"

set -ex

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TARGET_ARCH="$1"
[ -z "$TARGET_ARCH" ] && TARGET_ARCH="x86_64"

BUILD_DIR="$2"
[ -z "$BUILD_DIR" ] && BUILD_DIR="$SCRIPT_DIR"/target/"$TARGET_ARCH"/debug

cmake -E make_directory "$BUILD_DIR"
# build with dependencies of build_type Release
conan install -if="$BUILD_DIR" --build=missing --profile:build=default --profile:host="${SCRIPT_DIR}/toolchains/target_${TARGET_ARCH}_Release" "$SCRIPT_DIR"
cd "$BUILD_DIR" || exit
# shellcheck disable=SC1091
source activate.sh # Set environment variables for cross build
cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Debug -DSDV_COVERAGE=ON -DSDV_BUILD_TESTING=ON -DCONAN_CMAKE_SILENT_OUTPUT=ON -DCMAKE_INSTALL_PREFIX="./install"
sleep 1
cmake --build . -j
cmake --install .
