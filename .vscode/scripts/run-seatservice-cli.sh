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
# shellcheck disable=SC2034
# shellcheck disable=SC2086

echo "#######################################################"
echo "### Running Seatservice Client                      ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

# [ "$1" = "--task" ] && shift
POS="$1"
shift
ARGS="$*"

# handle environment sync flag from vs task
if [ "$SEAT_WAIT" = "wait" ] && ! echo "$ARGS" | grep -q "\-\-wait"; then
	ARGS="--wait $ARGS"
fi

# gRPC port of hvac service (no dapr!)
SEATSERVICE_PORT='50051'

SEATSERVICE_EXEC_PATH="$ROOT_DIRECTORY/seat_service/target/$PROCESSOR/release/install/bin"

if [ ! -x "$SEATSERVICE_EXEC_PATH/seat_svc_client" ]; then
	echo "seat_svc_client binary is missing: $SEATSERVICE_EXEC_PATH"
	exit 1
fi

#export DAPR_GRPC_PORT=$SEATSERVICE_GRPC_PORT
echo "$ $SEATSERVICE_EXEC_PATH/seat_svc_client $POS $ARGS"
"$SEATSERVICE_EXEC_PATH/seat_svc_client" "$POS" $ARGS # --target "127.0.0.1:$SEATSERVICE_PORT"
