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

WORKFLOWS_ROOT="$REPO_ROOT/.github/workflows"
# Update workflow versions.
sed -i -E "s/(^.*):v${VERSION_REGEX}(.*)$/\1:v${VERSION}/" \
	"$WORKFLOWS_ROOT/seat_service_build.yml" \
	"$WORKFLOWS_ROOT/seat_service_docu_build.yml" \
	"$WORKFLOWS_ROOT/seat_service_release.yml" \
	"$WORKFLOWS_ROOT/seat_service_seatctrl_test.yml"

DOCKERDEV_ROOT="$REPO_ROOT/.devcontainer"
# Update docker dev files.
sed -i -E "s/(^.*):v${VERSION_REGEX}(.*)$/\1:v${VERSION}/" \
	"$DOCKERDEV_ROOT/Dockerfile"

# Create release commit and tag it
#git commit -a -m "Release ${VERSION}"
#git tag -a "v${VERSION}" -m "Release ${VERSION}"
