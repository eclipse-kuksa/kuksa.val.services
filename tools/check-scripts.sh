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
# shellcheck disable=SC2044
# shellcheck disable=SC2230

DIR="$1"
[ -z "$DIR" ] && DIR="."

if [ -z "$(which shellcheck)" ]; then
	sudo apt-get install -y shellcheck
fi

ISSUES=""
for f in $(find "$DIR" -type f); do
	# skip some dirs...
	if echo "$f" | grep -q '/.git/\|/target/'; then
		continue
	fi
	if file "$f" | grep -q "shell script"; then
		if ! shellcheck "$f"; then
			ISSUES="$ISSUES\n  $f"
		fi
	fi
done

echo
echo -e "Scripts with issues: $ISSUES"
echo
