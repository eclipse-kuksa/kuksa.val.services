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
echo "### Running Databroker                              ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

DATABROKER_PORT='55555'
DATABROKER_GRPC_PORT='52001'

build_databroker() {
	local arch="$1"
	# TODO: sync with vehicle-data-brroker/README.md
	echo "-- Building databroker for [$PROCESSOR]..."
	if [ "$arch" = "aarch64" ] || [ "$arch" = "arm64" ]; then
		cd "$ROOT_DIRECTORY" || exit 1
		RUSTFLAGS='-C link-arg=-s' cross build --release --bins --examples --target aarch64-unknown-linux-gnu || exit 1
	fi
	if [ "$arch" = "x86_64" ]; then
		cd "$ROOT_DIRECTORY" || exit 1
		RUSTFLAGS='-C link-arg=-s' cargo build --release --bins --examples || exit 1
	fi
}

#Detect host environment (distinguish for Mac M1 processor)
if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	echo "Detected ARM architecture"
	PROCESSOR="aarch64"
	DATABROKER_EXEC_PATH="$ROOT_DIRECTORY/target/aarch64-unknown-linux-gnu/release"
else
	echo "Detected x86_64 architecture"
	PROCESSOR="x86_64"
	DATABROKER_EXEC_PATH="$ROOT_DIRECTORY/target/release"
fi

if [ ! -f "$DATABROKER_EXEC_PATH/vehicle-data-broker" ]; then
	echo "databroker binary is missing: $DATABROKER_EXEC_PATH"
	build_databroker "$PROCESSOR"
	file "$DATABROKER_EXEC_PATH/vehicle-data-broker" || exit 1
fi

export DAPR_GRPC_PORT=$DATABROKER_GRPC_PORT
# export RUST_LOG="info,databroker=debug,vehicle_data_broker=debug"
dapr run \
	--app-id vehicledatabroker \
	--app-protocol grpc \
	--app-port $DATABROKER_PORT \
	--dapr-grpc-port $DATABROKER_GRPC_PORT \
	--components-path $ROOT_DIRECTORY/.dapr/components \
	--config $ROOT_DIRECTORY/.dapr/config.yaml &
$DATABROKER_EXEC_PATH/vehicle-data-broker --address 0.0.0.0
