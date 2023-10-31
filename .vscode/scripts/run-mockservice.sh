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
echo "### Running Mock Service                            ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

### NOTE: MOCKSERVICE_* variables are defined in task-common.sh
# MOCKSERVICE_PORT='50052'
# MOCKSERVICE_GRPC_PORT='52005'
# MOCKSERVICE_DAPR_APP_ID='mockservice'
# VEHICLEDATABROKER_DAPR_APP_ID='vehicledatabroker'

# NOTE: use curent sidecar's grpc port, don't connect directly to sidecar of kdb (DATABROKER_GRPC_PORT)
export DAPR_GRPC_PORT=$MOCKSERVICE_GRPC_PORT

MOCKSERVICE_EXEC_PATH="$ROOT_DIRECTORY/mock_service"
if [ ! -f "$MOCKSERVICE_EXEC_PATH/mockservice.py" ]; then
	echo "Can't find $MOCKSERVICE_EXEC_PATH/mockservice.py"
	exit 1
fi

cd "$MOCKSERVICE_EXEC_PATH" || exit 1
pip3 install -q -r requirements.txt

echo
echo "*******************************************"
echo "* Mock Service app-id: $MOCKSERVICE_DAPR_APP_ID"
echo "* Mock Service APP port: $MOCKSERVICE_PORT"
echo "* Mock Service Dapr sidecar port: $MOCKSERVICE_GRPC_PORT"
echo "* DAPR_GRPC_PORT=$DAPR_GRPC_PORT"
echo "* metadata: [ VEHICLEDATABROKER_DAPR_APP_ID=$VEHICLEDATABROKER_DAPR_APP_ID ]"
echo "*******************************************"
echo

## uncomment for dapr debug logs
# DAPR_OPT="--enable-api-logging --log-level debug"1
DAPR_OPT="--log-level warn"
dapr run \
	--app-id $MOCKSERVICE_DAPR_APP_ID \
	--app-protocol grpc \
	--app-port $MOCKSERVICE_PORT \
	--dapr-grpc-port $MOCKSERVICE_GRPC_PORT \
	$DAPR_OPT \
	--components-path $ROOT_DIRECTORY/.dapr/components \
	--config $ROOT_DIRECTORY/.dapr/config.yaml \
	-- \
	python3 mockservice.py
