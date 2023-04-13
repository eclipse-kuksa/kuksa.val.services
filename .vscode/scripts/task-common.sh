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
# shellcheck disable=SC2086

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# path to json config with val component versions
export CONFIG_JSON="$SCRIPT_DIR/../../prerequisite_settings.json"

###
### Common dapr GRPC setups
###

# Databroker App port
export DATABROKER_PORT='55555'
# Databroker Dapr Sidecar gRPC port
export DATABROKER_GRPC_PORT='52001'
# databroker dapr app id
export VEHICLEDATABROKER_DAPR_APP_ID="vehicledatabroker"

# SeatService App port
export SEATSERVICE_PORT='50051'
# SeatService Dapr Sidecar gRPC port
export SEATSERVICE_GRPC_PORT='52002'
# SeatService dapr app id
export SEATSERVICE_DAPR_APP_ID="seatservice"

# HvacService App port
export HVACSERVICE_PORT='50052'
# HvacService Dapr Sidecar gRPC port
export HVACSERVICE_GRPC_PORT='52005'
# HvacService dapr app id
export HVACSERVICE_DAPR_APP_ID="hvacservice"

# feedercan Dapr Sidecar gRPC port
export FEEDERCAN_GRPC_PORT='52008'
# feedercan dapr app id
export FEEDERCAN_DAPR_APP_ID="feedercan"

export MOCKSERVICE_PORT='50053'
export MOCKSERVICE_GRPC_PORT='59009'
export MOCKSERVICE_DAPR_APP_ID="mockservice"

_check_prerequisite() {
	local MISSING_PKG=""
	if [ -z "$(which jq)" ]; then
		MISSING_PKG="$MISSING_PKG jq"
	fi
	if [ -z "$(which git)" ]; then
		MISSING_PKG="$MISSING_PKG git"
	fi
	if [ -z "$(which curl)" ]; then
		MISSING_PKG="$MISSING_PKG curl"
	fi
	if [ -z "$(which tar)" ]; then
		MISSING_PKG="$MISSING_PKG tar"
	fi
	if [ -z "$(which python3)" ]; then
		MISSING_PKG="$MISSING_PKG python3 python3-pip"
	fi
	if [ -n "$MISSING_PKG" ]; then
		echo "### Installing prerequisites: $MISSING_PKG"
		sudo apt-get -y update && sudo apt-get -qqy $MISSING_PKG
	fi

	if [ -z "$(which pytest)" ]; then
		pip3 install pytest
	fi
}

download_release() {
	local executable="$1"
	local download_url="$2"
	local binary_path="$3"
	local binary_name="$4"

	if [ -f "$executable" ]; then
		echo "  found: $executable"
		return 0
	fi
	echo "- Downloading from: $download_url"
	curl -s -o "$binary_path/$binary_name" --create-dirs -L -H "Accept: application/octet-stream" "$download_url"
	echo
	echo "- downloaded: $(file $binary_path/$binary_name)"

	tar -xf "$binary_path/$binary_name" -C "$binary_path"
	if [ ! -x "$executable" ]; then
		echo "$executable not found in $binary_path/$binary_name"
		return 1
	fi
	return 0
}

# helper for checking out git branch
git_checkout() {
	local repo="$1"
	local dir="$2"
	local branch="$3" # optional branch

	echo "### Checkout: $repo, branch: \"$branch\""

	local branch_opt=""
	if [ -n "$branch" ]; then
		branch_opt="-c advice.detachedHead=false --single-branch --branch $branch"
	fi

	git clone --quiet $branch_opt "$repo" "$dir"
}

# init on sourcing
_check_prerequisite

if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	echo "- Detected AArch64 architecture"
	PROCESSOR="aarch64"
	PROCESSOR_ALT="arm64"
else
	echo "- Detected x86_64 architecture"
	PROCESSOR="x86_64"
	PROCESSOR_ALT="amd64"
fi
echo
export PROCESSOR
export PROCESSOR_ALT
