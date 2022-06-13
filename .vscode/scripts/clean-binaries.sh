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

echo "#######################################################"
echo "### Clean binaries                                  ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

CLEAN_FILES="$ROOT_DIRECTORY/seat_service/target/x86_64/release/install"
CLEAN_FILES="$ROOT_DIRECTORY/seat_service/target/aarch64/release/install $CLEAN_FILES"

CLEAN_FILES="$ROOT_DIRECTORY/target/release/vehicle-data-* $CLEAN_FILES"
CLEAN_FILES="$ROOT_DIRECTORY/target/aarch64-unknown-linux-gnu/release/vehicle-data-* $CLEAN_FILES"

set -x
rm -rfv $CLEAN_FILES
