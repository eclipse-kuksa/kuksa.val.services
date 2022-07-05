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
echo "### Running Integration Tests                       ###"
echo "#######################################################"

set -e

ROOT_DIRECTORY=$(git rev-parse --show-toplevel)
# shellcheck source=/dev/null
source "$ROOT_DIRECTORY/.vscode/scripts/task-common.sh" "$@"

if [ "$USE_DAPR" = "0" ]; then
	echo
	echo "##### Integration test in Container mode (it-setup.sh)"
	echo
else
	echo
	echo "##### Integration test in Dapr mode (standalone)"
	echo
fi

pip3 install -q -r "${ROOT_DIRECTORY}/integration_test/requirements-dev.txt"
pip3 install -q -r "${ROOT_DIRECTORY}/integration_test/requirements.txt"
pip3 install -q -e "${ROOT_DIRECTORY}/integration_test/"
pip3 install -q -e "${ROOT_DIRECTORY}/hvac_service/"

set +e

if [ "$USE_DAPR" = "0" ]; then
	"${ROOT_DIRECTORY}/integration_test/it-setup.sh" init

	# ensure containers are re-created before test
	"${ROOT_DIRECTORY}/integration_test/it-setup.sh" cleanup
	"${ROOT_DIRECTORY}/integration_test/it-setup.sh" start
	echo
	# sleep is needed as sometimes feedercan was not able to register datapoints in time and integration test fails
	sleep 1
	"${ROOT_DIRECTORY}/integration_test/it-setup.sh" status --logs
	echo
fi

# prevents dumping "E0617: Fork support is only compatible with the epoll1 and poll polling strategies"
export GRPC_ENABLE_FORK_SUPPORT="false"

# export GRPC_TRACE=all
# export GRPC_VERBOSITY=DEBUG
# export PYTHONVERBOSE=1
# PYTEST_DEBUG="-v -s --log-cli-level=DEBUG"

cd "${ROOT_DIRECTORY}/integration_test" || exit 1
pytest -v $PYTEST_DEBUG \
	--log-file="${ROOT_DIRECTORY}/results/IntegrationTest/integration.log" --log-file-level=DEBUG \
	--asyncio-mode=auto "$@" \
	--override-ini junit_family=xunit1 --junit-xml="${ROOT_DIRECTORY}/results/IntegrationTest/junit.xml" \
	.

rc=$?

if [ "$USE_DAPR" = "0" ]; then
	echo
	if [ $rc -eq 0 ]; then
		"${ROOT_DIRECTORY}/integration_test/it-setup.sh" status
	else
		"${ROOT_DIRECTORY}/integration_test/it-setup.sh" status --logs
	fi
	echo
	# cleanup it containers and images
	"${ROOT_DIRECTORY}/integration_test/it-setup.sh" cleanup
fi

exit $rc
