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
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

### NOTE: SEATSERVICE_* variables are defined in task-common.sh#
#
# export SEATSERVICE_PORT='50051'
# export SEATSERVICE_GRPC_PORT='52002'
# export SEATSERVICE_DAPR_APP_ID="seatservice"
# export VEHICLEDATABROKER_DAPR_APP_ID="vehicledatabroker"

# NOTE: use dapr sidecar's grpc port, don't connect directly to sidecar of vdb (DATABROKER_GRPC_PORT)

export DAPR_GRPC_PORT=$SEATSERVICE_GRPC_PORT

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

SEATSERVICE_EXEC_PATH="$ROOT_DIRECTORY/seat_service/target/$PROCESSOR/release/install/bin"
if [ ! -x "$SEATSERVICE_EXEC_PATH/seat_service" ]; then
	echo "seat_service binary is missing: $SEATSERVICE_EXEC_PATH"
	build_seatservice "$PROCESSOR"
	file "$SEATSERVICE_EXEC_PATH/seat_service" || exit 1
fi

###############################################
### SeatService specific environment config ###
###############################################

export CAN="cansim"
# export SA_DEBUG=1
export SC_CTL=0
export SAE_STOP=0
# DataBrokerFeeder Debug level (0, 1, 2)
export DBF_DEBUG=0

### Uncomment for DEBUG
export SA_DEBUG=2
export DBF_DEBUG=3
export SEAT_DEBUG=1

# needed to override vdb address
export BROKER_ADDR="127.0.0.1:$DAPR_GRPC_PORT"

### Uncomment for direct connection to databroker
# export GRPC_TRACE=all,-timer,-timer_check
# export GRPC_VERBOSITY=DEBUG

echo
echo "*******************************************"
echo "* Seat Service app-id: $SEATSERVICE_DAPR_APP_ID"
echo "* Seat Service APP port: $SEATSERVICE_PORT"
echo "* Seat Service Dapr sidecar port: $SEATSERVICE_GRPC_PORT"
echo "* DAPR_GRPC_PORT=$DAPR_GRPC_PORT"
echo "* BROKER_ADDR=$BROKER_ADDR"
echo "* metadata: [ VEHICLEDATABROKER_DAPR_APP_ID=$VEHICLEDATABROKER_DAPR_APP_ID ]"
echo "*******************************************"
echo

## Uncomment for dapr debug logs
# DAPR_OPT="--enable-api-logging --log-level debug"
DAPR_OPT="--log-level warn"
dapr run \
	--app-id "$SEATSERVICE_DAPR_APP_ID" \
	--app-protocol grpc \
	--app-port $SEATSERVICE_PORT \
	--dapr-grpc-port $SEATSERVICE_GRPC_PORT \
	$DAPR_OPT \
	--components-path "$ROOT_DIRECTORY/.dapr/components" \
	--config "$ROOT_DIRECTORY/.dapr/config.yaml" \
	& # -- \
"$SEATSERVICE_EXEC_PATH/val_start.sh"
