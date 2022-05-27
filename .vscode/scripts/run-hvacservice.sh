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
echo "### Running HVAC Service                            ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

DATABROKER_GRPC_PORT='52001'

HVACSERVICE_PORT='50052'
HVACSERVICE_GRPC_PORT='52005'

HVACSERVICE_EXEC_PATH="$ROOT_DIRECTORY/hvac_service"
if [ ! -f "$HVACSERVICE_EXEC_PATH/hvacservice.py" ]; then
	echo "Can't find $HVACSERVICE_EXEC_PATH/hvacservice.py"
	exit 1
fi

cd "$HVACSERVICE_EXEC_PATH" || exit 1
pip3 install -q -r requirements.txt

export DAPR_GRPC_PORT=$DATABROKER_GRPC_PORT
export HVACSERVICE_DAPR_APP_ID='hvacservice'
export VEHICLEDATABROKER_DAPR_APP_ID='vehicledatabroker'

dapr run \
	--app-id $HVACSERVICE_DAPR_APP_ID \
	--app-protocol grpc \
	--app-port $HVACSERVICE_PORT \
	--dapr-grpc-port $HVACSERVICE_GRPC_PORT \
	--components-path $ROOT_DIRECTORY/.dapr/components \
	--config $ROOT_DIRECTORY/.dapr/config.yaml &
sleep 1 && python3 ./hvacservice.py
