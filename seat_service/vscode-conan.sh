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
# shellcheck disable=SC1090
# shellcheck disable=SC1091
# shellcheck disable=SC2181

# Script for vs code with initialized conan environment (debug)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$1" = "-h" ] || [ "$1" == "--help" ]; then
	echo "Usage: $0 {-h} {project_dir} {cmake_build_dir}"
	echo
	echo "Description:"
	echo "  Launcher script for vs.code with pre-initialized conan environment to work with cmake in vs.code"
	echo
	echo "Arguments:"
	echo "  project_dir      vs.code project directory (default: ..)"
	echo "  cmake_build_dir  vs.code cmake directory (default: ./build)"
	echo
	exit 0
fi

PROJ_DIR="$1"
[ -z "$PROJ_DIR" ] && PROJ_DIR="$SCRIPT_DIR/.."

BUILD_DIR="$2"
[ -z "$BUILD_DIR" ] && BUILD_DIR="$PROJ_DIR/build"

conan install -if="$BUILD_DIR" --build=missing --profile:build=default --profile:host="$SCRIPT_DIR/toolchains/target_x86_64_Debug" "$SCRIPT_DIR"
[ $? -ne 0 ] && echo "conan install failed!" && exit 1

if [ -f "$BUILD_DIR/activate.sh" ]; then
	source "$BUILD_DIR/activate.sh"
	echo
	echo "Executing: code $PROJ_DIR with cmake/conan environment..."
	echo
	code "$PROJ_DIR"
else
	echo "Can't find $BUILD_DIR/activate.sh !"
	exit 1
fi
