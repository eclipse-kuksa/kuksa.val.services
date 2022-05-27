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
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

DATABROKER_GRPC_PORT='52001'

# Downloading feedercan
FEEDERCAN_EXEC_PATH="$ROOT_DIRECTORY/feeder_can"

if [ ! -f "$FEEDERCAN_EXEC_PATH/dbcfeeder.py" ]; then
	echo "Can't find $FEEDERCAN_EXEC_PATH/dbcfeeder.py"
	exit 1
fi

cd "$FEEDERCAN_EXEC_PATH" || exit 1
pip3 install -q -r requirements.txt

export DAPR_GRPC_PORT="$DATABROKER_GRPC_PORT"
export VEHICLEDATABROKER_DAPR_APP_ID="vehicledatabroker"
export LOG_LEVEL="info,databroker=info,dbcfeeder.broker_client=debug,dbcfeeder=info"
dapr run \
	--app-id feedercan \
	--app-protocol grpc \
	--components-path $ROOT_DIRECTORY/.dapr/components \
	--config $ROOT_DIRECTORY/.dapr/config.yaml &
./dbcfeeder.py
