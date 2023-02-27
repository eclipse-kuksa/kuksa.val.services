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

### NOTE: DATABROKER_* variables are defined in task-common.sh
# # Databroker App port
# DATABROKER_PORT='55555'
# # Databroker Dapr Sidecar gRPC port
# DATABROKER_GRPC_PORT='52001'
# # Dapr app id for databroker
# VEHICLEDATABROKER_DAPR_APP_ID='vehicledatabroker'

export DAPR_GRPC_PORT=$DATABROKER_GRPC_PORT

DATABROKER_VERSION=$(jq -r '.databroker.version // empty' "$CONFIG_JSON")
if [ -z "$DATABROKER_VERSION" ]; then
	echo "Coudln't find databroker version from $CONFIG_JSON"
	exit 1
fi

# https://github.com/eclipse/kuksa.val/releases/download/0.3.0/databroker-amd64.tar.gz
DATABROKER_BINARY_NAME="databroker-$PROCESSOR_ALT.tar.gz"
DATABROKER_BINARY_PATH="$ROOT_DIRECTORY/.vscode/scripts/assets/databroker/$DATABROKER_VERSION/$PROCESSOR_ALT"
DATABROKER_EXECUTABLE="$DATABROKER_BINARY_PATH/databroker/databroker"
DOWNLOAD_URL="https://github.com/eclipse/kuksa.val/releases/download/$DATABROKER_VERSION/$DATABROKER_BINARY_NAME"

download_release "$DATABROKER_EXECUTABLE" "$DOWNLOAD_URL" "$DATABROKER_BINARY_PATH" "$DATABROKER_BINARY_NAME" || exit 1

### Data Broker environment setup ###

## Uncomment for feed values debug
export RUST_LOG="debug,databroker=debug,vehicle_data_broker=debug,h2=info"
##export GRPC_TRACE=all,-timer,-timer_check
##export GRPC_VERBOSITY=DEBUG

## Uncomment to preregister vehicle model metadata in vss.json
#DATABROKER_METADATA="--metadata $ROOT_DIRECTORY/.vscode/scripts/vss.json"

echo
echo "*******************************************"
echo "* Kuksa Data Broker app-id: $VEHICLEDATABROKER_DAPR_APP_ID"
echo "* Kuksa Data Broker APP port: $DATABROKER_PORT"
echo "* Kuksa Data Broker Dapr sidecar port: $DATABROKER_GRPC_PORT"
echo "* DAPR_GRPC_PORT=$DAPR_GRPC_PORT"
echo "* Dapr metadata: [ VEHICLEDATABROKER_DAPR_APP_ID=$VEHICLEDATABROKER_DAPR_APP_ID ]"
[ -n "$DATABROKER_METADATA" ] && echo "* Kuksa Data Broker Metadata: $DATABROKER_METADATA"
echo "*******************************************"
echo

## Uncomment for dapr debug logs
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
$DATABROKER_EXECUTABLE --address 0.0.0.0 $DATABROKER_METADATA
