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
set -e

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 <version>"
	exit 1
fi

VERSION_REGEX="[0-9]+\.[0-9]+(\.[0-9]+)?"
VERSION="$1"

if [ "$(echo "$1" | sed -E "s/$VERSION_REGEX//")" ]; then
	echo "<version> should be of the form MAJOR.MINOR[.PATCH]"
	exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

DOCKERDEV_ROOT="$REPO_ROOT/.devcontainer"

# replace python package versions
PYTHON_ROOT="$REPO_ROOT"
sed -i -E "s/^[[:space:]]*PKG_VERSION[[:space:]]*=[[:space:]]*\"(.*)\"\$/PKG_VERSION = \"v${VERSION}\"/" \
	"$PYTHON_ROOT/integration_test/setup.py" \
	"$PYTHON_ROOT/hvac_service/setup.py"

