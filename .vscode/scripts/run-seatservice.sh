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
echo "### Running Seatservice                             ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

SEATSERVICE_PORT='50051'
SEATSERVICE_GRPC_PORT='52002'

build_seatservice() {
	local arch="$1"
	echo "-- Building databroker for [$PROCESSOR]..."
	cd "$ROOT_DIRECTORY/seat_service" || exit 1
	if [ "$arch" = "aarch64" ] || [ "$arch" = "arm64" ]; then
		./build-release.sh "aarch64" # only aarch64 is supported
	fi
	if [ "$arch" = "x86_64" ]; then
		./build-release.sh "x86_64"
	fi
}

#Detect host environment (distinguish for Mac M1 processor)
if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	echo "Detected ARM architecture"
	PROCESSOR="aarch64"
else
	echo "Detected x86_64 architecture"
	PROCESSOR="x86_64"
fi

SEATSERVICE_EXEC_PATH="$ROOT_DIRECTORY/seat_service/target/$PROCESSOR/release/install/bin"

if [ ! -x "$SEATSERVICE_EXEC_PATH/seat_service" ]; then
	echo "seat_service binary is missing: $SEATSERVICE_EXEC_PATH"
	build_seatservice "$PROCESSOR"
	file "$SEATSERVICE_EXEC_PATH/seat_service" || exit 1
fi

export DAPR_GRPC_PORT=$SEATSERVICE_GRPC_PORT
export CAN="cansim"
export VEHICLEDATABROKER_DAPR_APP_ID="vehicledatabroker"
# needed to override vdb address
export BROKER_ADDR="127.0.0.1:$DAPR_GRPC_PORT"
# export SA_DEBUG=1
# export SC_STAT=1

dapr run \
	--app-id seatservice \
	--app-protocol grpc \
	--app-port $SEATSERVICE_PORT \
	--dapr-grpc-port $SEATSERVICE_GRPC_PORT \
	--components-path $ROOT_DIRECTORY/.dapr/components \
	--config $ROOT_DIRECTORY/.dapr/config.yaml &
$SEATSERVICE_EXEC_PATH/val_start.sh
