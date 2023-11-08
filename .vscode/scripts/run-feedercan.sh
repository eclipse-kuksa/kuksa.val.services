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

# NOTE: [config] feedercan.version is git tag for standalone tests, feedercan.imageVersion is for container tests
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

echo "### Starting from: $FEEDERCAN_EXEC_PATH"

if [ ! -f "$FEEDERCAN_EXEC_PATH/dbcfeeder.py" ]; then
	[ -d "$FEEDERCAN_DIR" ] || mkdir -p "$FEEDERCAN_DIR"
	# decide if we want to checkout a branch, or download release sources
	git_checkout "$FEEDERCAN_REPO" "$FEEDERCAN_DIR" "$FEEDERCAN_VERSION"
fi

cd "$FEEDERCAN_EXEC_PATH" || exit 1

PYTHON_BIN="python3.9"
PIP3_BIN="pip3.9"

if [ -z "$(which python3.9)" ]; then
	echo "### WARNING! dbc2val requires python3.9" 1>&2
	echo "You may install 3.9 in pyenv. Check: https://github.com/pyenv/pyenv#installation"
	echo "Or using apt:"
	echo "  sudo apt-get install -y python3.9"
	echo "  curl https://bootstrap.pypa.io/get-pip.py | python3.9"
	echo "  python3.9 --version"
	echo "  pip3.9 --version"
	echo
	# fallback to default python (e.g. it may be 9.10)
	PYTHON_BIN="python3"
	PIP3_BIN="pip3"
fi

# PIP_OPT="--upgrade --retries 1 --timeout 3"
### IMPORTANT: dbc2val now requires python 3.9, does not work on python 3.8
"$PIP3_BIN" install $PIP_OPT -q -r requirements.txt -r requirements-dev.txt

####################################
### feedercan environment setup ####
####################################

# Dir with integration test specific config and data for feedercan
CONFIG_DIR="$ROOT_DIRECTORY/integration_test/volumes/dbc2val"

### Override default files for feedercan to be consistent with tests
export DBC_FILE="$CONFIG_DIR/it-can.dbc"
export MAPPING_FILE="$CONFIG_DIR/it-vss_4.0.json"
# uncomment if you need to test with DogMode datapoints (requires custom vss config for databroker)
#export MAPPING_FILE="$CONFIG_DIR/it-vss_4.0-dogmode.json"
export CANDUMP_FILE="$CONFIG_DIR/it-candump0.log"

# export USECASE="databroker"
export USE_DBC2VAL=1
export NO_USE_VAL2DBC=1

export LOG_LEVEL="info" #,dbcfeeder=debug"
## Uncomment to enable most of the debug modules
#export LOG_LEVEL="debug,dbcfeeder=debug,kuksa_client.grpc=debug,dbcfeederlib.canplayer=debug,dbcfeederlib.canreader=debug,dbcfeederlib.dbc2vssmapper=debug"

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
	--resources-path "$ROOT_DIRECTORY/.dapr/components" \
	--config "$ROOT_DIRECTORY/.dapr/config.yaml" &
#--
"$PYTHON_BIN" -u ./dbcfeeder.py
