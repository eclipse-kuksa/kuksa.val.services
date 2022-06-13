#!/bin/sh
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

find ./src -path ./src/lib/seat_adjuster/seat_controller -print -prune -o -iname './*.h' -exec clang-format --style=file -i {} \;
find ./src -path ./src/lib/seat_adjuster/seat_controller -print -prune -o -iname './*.cc' -exec clang-format --style=file -i {} \;
find ./src -path ./src/lib/seat_adjuster/seat_controller -print -prune -o -iname './*.c' -exec clang-format --style=file -i {} \;
