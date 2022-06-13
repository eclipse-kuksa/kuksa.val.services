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

DATABROKER_VERSION="test"

#Detect host environment (distinguish for Mac M1 processor)
if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	echo "Detected AArch64 architecture"
	PROCESSOR="aarch64"
else
	echo "Detected x86_64 architecture"
	PROCESSOR="x86_64"
fi
DATABROKER_BINARY_NAME="bin_release_databroker_$PROCESSOR.tar.gz"
DATABROKER_BINARY_PATH="$ROOT_DIRECTORY/.vscode/scripts/assets/databroker/$DATABROKER_VERSION/$PROCESSOR"
DATABROKERCLI_EXECUTABLE="$DATABROKER_BINARY_PATH/target/release/vehicle-data-cli"

DOWNLOAD_URL=https://github.com/eclipse/kuksa.val/releases/download/$DATABROKER_VERSION/$DATABROKER_BINARY_NAME

if [[ ! -f "$DATABROKERCLI_EXECUTABLE" ]]; then
	echo "Downloading vehicle-data-broker:$DATABROKER_VERSION"
	curl -o "$DATABROKER_BINARY_PATH"/"$DATABROKER_BINARY_NAME" --create-dirs -L -H "Accept: application/octet-stream" "$DOWNLOAD_URL"
	tar -xf "$DATABROKER_BINARY_PATH"/"$DATABROKER_BINARY_NAME" -C "$DATABROKER_BINARY_PATH"
fi

"$DATABROKERCLI_EXECUTABLE"
