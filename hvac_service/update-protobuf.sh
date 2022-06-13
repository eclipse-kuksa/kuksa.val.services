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

PROTO_DIRS=(
	"../vehicle_data_broker/proto"
	"./proto"
)

for src_dir in "${PROTO_DIRS[@]}"; do
	if [ ! -d "$src_dir" ]; then
		echo "Error! Proto dir does not exist: $src_dir"
		exit 1
	fi
done

# make sure deps are installed
echo "# Installing requirements.txt ..."
pip3 install -q -r requirements-dev.txt

set -xe
PROTO_FILES=$(find "${PROTO_DIRS[@]}" -name '*.proto')

printf -v PROTO_PATH "%s:" "${PROTO_DIRS[@]}"
PROTO_PATH="${PROTO_PATH%:}"

echo "# Generating grpc stubs from: ${PROTO_PATH} ..."
python3 -m grpc_tools.protoc \
	--python_out=. \
	--grpc_python_out=. \
	--proto_path="${PROTO_PATH}" \
	$PROTO_FILES
set +xe

echo "# Ensure each generated folder contains an __init__.py ..."
# Get root package names
# shellcheck disable=SC2068 # Double quotes don't work with grep
ROOT_PACKAGES=$(grep -Poshr "^package[[:space:]]+\K[_0-9A-Za-z]+" ${PROTO_FILES[@]})
# Remove duplicates
IFS=" " read -r -a ROOT_PACKAGES <<<"$(tr ' ' '\n' <<<"${ROOT_PACKAGES[@]}" | sort -u | tr '\n' ' ')"
# Recursively add __init__.py files
find "${ROOT_PACKAGES[@]}" -type d -exec touch {}/"__init__.py" \;

echo "# Generated files:"
find "${ROOT_PACKAGES[@]}" -type f -name '*.py'
