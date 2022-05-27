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
# shellcheck disable=SC2181

DIR="$1"

[ -z "$DIR" ] && DIR="."

COL_NC='\e[39m'
COL_RED='\e[31m'
COL_GREEN='\e[32m'
COL_YELLOW='\e[33m'

# ignore some known dirs/files
FILES=$(find "$DIR" -type f | grep -v \
	'.md$\|.tar$\|.tar.gz$\|.my$\|.git\|.lock$\|.dockerignore$\|.svg$\|.drawio$\|/build\|target/\|doxygen/\|.tox\|.mypy\|__pycache__/\|.pytest_cache\|.json$\|.pyi$\|__init__.py\|gen_proto/\|.egg-info')

MISSING=""
for f in $FILES; do
	LICENSE=$(grep "SPDX-License-Identifier:" "$f" | grep -v 'LICENSE=\|SPDX=') ## exclude refs in $0
	if [ $? -eq 0 ]; then
		SPDX=$(echo "${LICENSE/#*SPDX-License-Identifier: /}" | tr -d '\r')
		if [ "$SPDX" = "Apache-2.0" ]; then
			col="${COL_GREEN}"
		else
			col="${COL_YELLOW}"
		fi
		printf "${col}%20s${COL_NC} | %s\n" "$SPDX" "$f"
	else
		printf "${COL_RED}%20s${COL_NC} | ${COL_RED}%s${COL_NC}\n" "Not Found!" "$f"
		MISSING=$(printf '%s\n  %s' "$MISSING" "$f")
	fi
done

[ -n "$MISSING" ] && printf "\n### Missing licenses in:\n${COL_RED}%s${COL_NC}\n" "$MISSING"
