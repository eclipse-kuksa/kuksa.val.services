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
DAPR_HVAC_SVC="hvacservice"

set -e

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

if ! __check_dapr_app "$DAPR_VDB" || ! __check_dapr_app "$DAPR_HVAC_SVC"; then
	echo "Please run vs-code tasks: [run-databroker, run-hvacservice]"
	exit 10
fi

temp="$1"
mode="$2"

echo "### Setting HVAC mode: ${mode}, temp: ${temp}"
$SCRIPT_DIR/../.vscode/scripts/run-hvac-cli.sh "$temp" "$mode"
exit $?
