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
# shell check disable=SC2002
# shell check disable=SC2086

echo "#######################################################"
echo "### Running Integration Tests                       ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/exec-check.sh" "$@"

pip install -q -r "${ROOT_DIRECTORY}/integration_test/requirements-dev.txt"
pip install -q -e "${ROOT_DIRECTORY}/integration_test/"

pytest -v "${ROOT_DIRECTORY}/integration_test" \
	--log-file=./results/IntegrationTest/integration.log \
	--asyncio-mode=auto --override-ini \
	junit_family=xunit1 --junit-xml=./results/IntegrationTest/junit.xml

exit $?
