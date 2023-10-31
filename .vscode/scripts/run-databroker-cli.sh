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
echo "### Running DataBroker CLI                          ###"
echo "#######################################################"

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

DATABROKER_VERSION=$(jq -r '.databroker.version // empty' "$CONFIG_JSON")
if [ -z "$DATABROKER_VERSION" ]; then
	echo "Coudln't find databroker version from $CONFIG_JSON"
	exit 1
fi

# https://github.com/eclipse/kuksa.val/releases/download/0.3.0/databroker-cli-amd64.tar.gz

DATABROKER_BINARY_ZIP="databroker-cli-$PROCESSOR_ALT.zip"
DATABROKER_BINARY_NAME="databroker-cli-$PROCESSOR_ALT.tar.gz"

DOWNLOAD_URL="https://github.com/eclipse/kuksa.val/releases/download/$DATABROKER_VERSION/$DATABROKER_BINARY_ZIP"
#DOWNLOAD_URL="https://github.com/eclipse/kuksa.val/releases/download/$DATABROKER_VERSION/$DATABROKER_BINARY_NAME"

DATABROKER_BINARY_NAME="databroker-cli-$PROCESSOR_ALT.tar.gz"
DATABROKER_BINARY_PATH="$ROOT_DIRECTORY/.vscode/scripts/assets/databroker/$DATABROKER_VERSION/$PROCESSOR_ALT"
DATABROKERCLI_EXECUTABLE="$DATABROKER_BINARY_PATH/databroker-cli/databroker-cli"

download_zip_release "$DATABROKERCLI_EXECUTABLE" "$DOWNLOAD_URL" "$DATABROKER_BINARY_PATH" "$DATABROKER_BINARY_ZIP" "$DATABROKER_BINARY_NAME" || exit 1

"$DATABROKERCLI_EXECUTABLE"