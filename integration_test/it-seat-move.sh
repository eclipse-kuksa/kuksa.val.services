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

# parse 1st arg (optional as pos)
pos="$1"
[ -z "$pos" ] && pos="500"

# pass extra args, e.g. "--wait" to wait reaching desired position
shift
args="$*"

exec_seat_client() {
	local pos="$1"
	shift
	local extra_args="$*"
	#echo "#[$$]# Moving Seat to ${pos} ..."
	echo "$ docker exec ${SEAT_CONTAINER} /app/bin/seat_svc_client ${extra_args} ${pos}"
	docker exec "${SEAT_CONTAINER}" /app/bin/seat_svc_client ${extra_args} "${pos}"
	#echo "#[$$]# Moving Seat to ${pos}. Done!"
}

### Checks if container (name) is running
__check_container_state() {
	local name="$1"
	local verbose="$2"
	RESULT=$(docker ps -a --filter name=${name} --format='{{.Names}}\t{{.Image}}\tstate:{{.State}}\t{{.Status}}\tNetwork:{{.Networks}} ({{.Ports}})')
	[ "$verbose" = "1" ] && echo "$RESULT" 1>&2
	if echo "$RESULT" | grep -q "state:running"; then
		return 0
	else
		return 1
	fi
}

${SCRIPT_DIR}/it-setup.sh start

if ! __check_container_state ${VDB_CONTAINER} ||
	! __check_container_state "${SEAT_CONTAINER}"; then
	echo "Containers for IntegrationTest test are not running!"
	exit 10
fi

echo "### Moving SEAT to ${pos}"
exec_seat_client ${args} "$pos"
exit $?
