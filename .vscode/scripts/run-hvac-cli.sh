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
echo "### Running HVAC Client                             ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

[ "$1" = "--task" ] && shift

TEMP="$1"
STATUS_MODE="$2"

# sanity checks for invalid user input
if [ -z "$TEMP" ] || [ "$STATUS_MODE" != "ON" ] && [ "$STATUS_MODE" != "OFF" ]; then
	echo "Invalid arguments!"
	echo
	echo "Usage: $0 --task AC_TEMP [ON | OFF]"
	echo
	exit 1
fi

# replace [ON/OFF] with [1/0] for AC_STATUS
if [ "$STATUS_MODE" = "ON" ]; then
	STATUS="1"
else
	STATUS="0"
fi

HVACSERVICE_PORT='50052'
HVACSERVICE_EXEC_PATH="$ROOT_DIRECTORY/hvac_service"
if [ ! -f "$HVACSERVICE_EXEC_PATH/testclient.py" ]; then
	echo "Can't find $HVACSERVICE_EXEC_PATH/testclient.py"
	exit 1
fi

cd "$HVACSERVICE_EXEC_PATH" || exit 1
pip3 install -q -r requirements.txt

set -x
python3 -u testclient.py --addr=localhost:$HVACSERVICE_PORT --temp=$TEMP --status=$STATUS
