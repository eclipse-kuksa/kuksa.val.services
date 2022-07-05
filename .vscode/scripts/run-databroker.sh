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
# shellcheck disable=SC2181

echo "#######################################################"
echo "### Running Databroker                              ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

DATABROKER_VERSION=$(jq -r '.databroker.version // empty' "$CONFIG_JSON")
if [ -z "$DATABROKER_VERSION" ]; then
	echo "Coudln't find databroker version from $CONFIG_JSON"
	exit 1
fi

# # Databroker App port
# DATABROKER_PORT='55555'
# # Databroker Dapr Sidecar gRPC port
# DATABROKER_GRPC_PORT='52001'
#VEHICLEDATABROKER_DAPR_APP_ID='vehicledatabroker'

DATABROKER_BINARY_NAME="databroker_$PROCESSOR.tar.gz"
DATABROKER_BINARY_PATH="$ROOT_DIRECTORY/.vscode/scripts/assets/databroker/$DATABROKER_VERSION/$PROCESSOR"
DATABROKER_EXECUTABLE="$DATABROKER_BINARY_PATH/target/release/databroker"
DOWNLOAD_URL="https://github.com/eclipse/kuksa.val/releases/download/$DATABROKER_VERSION/$DATABROKER_BINARY_NAME"

download_release "$DATABROKER_EXECUTABLE" "$DOWNLOAD_URL" "$DATABROKER_BINARY_PATH" "$DATABROKER_BINARY_NAME" || exit 1

export DAPR_GRPC_PORT=$DATABROKER_GRPC_PORT
# export RUST_LOG="info,databroker=debug,vehicle_data_broker=debug"

## uncomment for dapr debug logs
# DAPR_OPT="--enable-api-logging --log-level debug"

dapr run \
	--app-id $VEHICLEDATABROKER_DAPR_APP_ID \
	--app-protocol grpc \
	--app-port $DATABROKER_PORT \
	--dapr-grpc-port $DATABROKER_GRPC_PORT \
	$DAPR_OPT \
	--components-path $ROOT_DIRECTORY/.dapr/components \
	--config $ROOT_DIRECTORY/.dapr/config.yaml \
	& # -- \
	$DATABROKER_EXECUTABLE --address 0.0.0.0
	# --metadata $ROOT_DIRECTORY/.vscode/scripts/vss.json
