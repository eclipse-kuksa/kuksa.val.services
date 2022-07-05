#!/bin/bash
# /********************************************************************************
# * Copyright (c) 2022 Contributors to the Eclipse Foundation
# *
# * See the NOTICE file(s) distributed with this work for additional
# * information regarding copyright ownership.
# *
# * This program and the accompanying materials are made available under the
# * terms of the Apache License 2.0 which is available at
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * SPDX-License-Identifier: Apache-2.0
# ********************************************************************************/

# shellcheck disable=SC2181
# shellcheck disable=SC2086
# shellcheck disable=SC2230

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

set -e
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/it-config"

TEMP="$1"
STATUS_MODE="$2"

# replace [ON/OFF] with [1/0] for AC_STATUS
if [ "${STATUS_MODE}" = "ON" ]; then
	STATUS="1"
elif [ "${STATUS_MODE}" = "OFF" ]; then
	STATUS="0"
else
	echo "Usage: $0 [temp] [ON | OFF]"
	exit 1
fi

### Checks if container (name) is running
__check_container_state() {
	local name="$1"
	local verbose="$2"
	RESULT=$(docker ps -a --filter name=${name} --format='{{.Names}}\t{{.Image}}\tstate:{{.State}}\t{{.Status}}\tNetwork:{{.Networks}} ({{.Ports}})')
	[ "${verbose}" = "1" ] && echo "${RESULT}" 1>&2
	if echo "${RESULT}" | grep -q "state:running"; then
		return 0
	else
		return 1
	fi
}

# start was reworked to do it "lazy", otherwise start was initially stopping containers and hvac may not be available just after startup
"${SCRIPT_DIR}/it-setup.sh" start

if ! __check_container_state "${VDB_CONTAINER}" || ! __check_container_state "${HVAC_CONTAINER}"
then
	echo "Containers for IntegrationTest test are not running!"
	exit 10
fi

cd "${SCRIPT_DIR}/../hvac_service" || exit 1

## NOTE: Might need some time after container startup to open the service port...
# netstat -avpnt | grep ${HVAC_HOST_PORT}
# sleep 1

echo "### Setting HVAC temp:${TEMP}, status:${STATUS}, port: ${HVAC_HOST_PORT}"
python -u testclient.py --temp "${TEMP}" --status "${STATUS}" --addr "localhost:${HVAC_HOST_PORT}"
exit $?
