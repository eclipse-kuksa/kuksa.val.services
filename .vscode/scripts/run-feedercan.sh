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
# shellcheck disable=SC2086

echo "#######################################################"
echo "### Running FeederCan                               ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)

# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

### NOTE: FEEDERCAN_* variables are defined in task-common.sh
#
# FEEDERCAN_GRPC_PORT='52008'
# FEEDERCAN_DAPR_APP_ID="feedercan"

# NOTE: use dapr sidecar's grpc port, don't connect directly to sidecar of kdb (DATABROKER_GRPC_PORT)
export DAPR_GRPC_PORT="$FEEDERCAN_GRPC_PORT"

FEEDERCAN_VERSION=$(jq -r '.feedercan.version // empty' "$CONFIG_JSON")

### Default feedercan settings (repo, tag are optional in json)
FEEDERCAN_REPO="https://github.com/eclipse/kuksa.val.feeders.git"
if [ -z "$FEEDERCAN_VERSION" ]; then
	echo "Coudln't find feedercan version from $CONFIG_JSON"
	exit 1
fi

# feedercan local asset path (with path-safe tag name)
FEEDERCAN_DIR="$ROOT_DIRECTORY/.vscode/scripts/assets/feedercan/${FEEDERCAN_VERSION//[^a-zA-Z0-9._-]/#}"
# path to dbcfeeder within the project
FEEDERCAN_EXEC_PATH="$FEEDERCAN_DIR/dbc2val"

if [ ! -f "$FEEDERCAN_EXEC_PATH/dbcfeeder.py" ]; then
	[ -d "$FEEDERCAN_DIR" ] || mkdir -p "$FEEDERCAN_DIR"
	# decide if we want to checkout a branch, or download release sources
	git_checkout "$FEEDERCAN_REPO" "$FEEDERCAN_DIR" "$FEEDERCAN_VERSION"
fi

cd "$FEEDERCAN_EXEC_PATH" || exit 1
pip3 install -q -r requirements.txt

####################################
### feedercan environment setup ####
####################################

# Dir with integration test specific config and data for feedercan
CONFIG_DIR="$ROOT_DIRECTORY/integration_test/volumes/dbc2val"

### Override default files for feedercan to be consistent with tests
export DBC_FILE="$CONFIG_DIR/it-can.dbc"
export MAPPING_FILE="$CONFIG_DIR/it-mapping.yml"
export CANDUMP_FILE="$CONFIG_DIR/it-candump0.log"
export USECASE="databroker"

## Uncomment to enable most of the debug modules
# export LOG_LEVEL="info,databroker=debug,dbcfeeder.broker_client=debug,dbcfeeder=debug,dbcreader=debug"
## Uncomment for verbose dbcreader decoded can dumps, needs dbcreader=debug
# export CAN_VERBOSE=1

echo
echo "*******************************************"
echo "* dbc2val Dapr app-id: $FEEDERCAN_DAPR_APP_ID"
echo "* dbc2val APP port: $FEEDERCAN_GRPC_PORT"
echo "* dbc2val Dapr sidecar port: $FEEDERCAN_GRPC_PORT"
echo "* DAPR_GRPC_PORT=$DAPR_GRPC_PORT"
echo "* Dapr metadata: [ VEHICLEDATABROKER_DAPR_APP_ID=$VEHICLEDATABROKER_DAPR_APP_ID ]"
echo "*******************************************"
echo

## Uncomment for dapr debug logs
# DAPR_OPT="--enable-api-logging --log-level debug"
DAPR_OPT="--log-level warn"
dapr run \
	--app-id "$FEEDERCAN_DAPR_APP_ID" \
	--app-protocol grpc \
	--dapr-grpc-port $FEEDERCAN_GRPC_PORT \
	$DAPR_OPT \
	--components-path "$ROOT_DIRECTORY/.dapr/components" \
	--config "$ROOT_DIRECTORY/.dapr/config.yaml" &
#--
python3 -u ./dbcfeeder.py
