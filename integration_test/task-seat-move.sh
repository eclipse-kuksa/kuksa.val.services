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

DAPR_VDB="vehicledatabroker"
DAPR_SEATSVC="seatservice"

set -e

# parse 1st arg (optional as pos)
pos="$1"
[ -z "$pos" ] && pos="500"

# pass extra args, e.g. "--wait" to wait reaching desired position
shift
args="$*"

### Checks if dapr application (name) is running
__check_dapr_app() {
	local name="$1"
	RESULT=$(dapr list | grep "$name")
	if [ -n "$RESULT" ]; then
		return 0
	else
		return 1
	fi
}

if ! __check_dapr_app "$DAPR_VDB" || ! __check_dapr_app "$DAPR_SEATSVC"; then
	echo "Please run vs-code tasks: [run-vehicledatabroker, run-seatservice]"
	exit 10
fi

echo "### Moving SEAT to ${pos}"
$SCRIPT_DIR/../.vscode/scripts/run-seatservice-cli.sh --task "$pos" $args
exit $?
