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
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

DATABROKER_VERSION=$(jq -r '.databroker.version // empty' "$CONFIG_JSON")
if [ -z "$DATABROKER_VERSION" ]; then
	echo "Coudln't find databroker version from $CONFIG_JSON"
	exit 1
fi

DATABROKER_BINARY_NAME="databroker_$PROCESSOR.tar.gz"
DATABROKER_BINARY_PATH="$ROOT_DIRECTORY/.vscode/scripts/assets/databroker/$DATABROKER_VERSION/$PROCESSOR"
DOWNLOAD_URL="https://github.com/eclipse/kuksa.val/releases/download/$DATABROKER_VERSION/$DATABROKER_BINARY_NAME"
DATABROKERCLI_EXECUTABLE="$DATABROKER_BINARY_PATH/target/release/databroker-cli"

download_release "$DATABROKERCLI_EXECUTABLE" "$DOWNLOAD_URL" "$DATABROKER_BINARY_PATH" "$DATABROKER_BINARY_NAME" || exit 1

"$DATABROKERCLI_EXECUTABLE"
