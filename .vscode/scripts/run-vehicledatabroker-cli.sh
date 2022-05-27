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
# shellcheck disable=SC2002

echo "#######################################################"
echo "### Running VehicleDataBroker CLI                   ###"
echo "#######################################################"

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

#Detect host environment (distinguish for Mac M1 processor)
if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	echo "Detected ARM architecture"
	DATABROKER_EXEC_PATH="$ROOT_DIRECTORY/target/aarch64-unknown-linux-gnu/release"
else
	echo "Detected x86_64 architecture"
	DATABROKER_EXEC_PATH="$ROOT_DIRECTORY/target/release"
fi

if [ ! -f "$DATABROKER_EXEC_PATH/vehicle-data-broker" ]; then
	echo "databroker binary is missing: $DATABROKER_EXEC_PATH"
	build_databroker "$PROCESSOR"
	file "$DATABROKER_EXEC_PATH/vehicle-data-broker" || exit 1
fi

"$DATABROKER_EXEC_PATH/vehicle-data-cli"
