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

#e cho "###[check] $* [$#]"
if [ $# -eq 0 ]; then
	tput setaf 1
	echo "ERROR: To execute script, use VSCODE Tasks: [CTRL+SHIFT+P -> Tasks: Run Tasks -> $1]."
	read -r -p "Press <Enter> to close this window"
	exit 1
fi
