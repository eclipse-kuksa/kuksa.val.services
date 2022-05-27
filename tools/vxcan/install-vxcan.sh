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
# shellcheck disable=SC2230

[ -f vxcan.c ] || wget "https://raw.githubusercontent.com/torvalds/linux/master/drivers/net/can/vxcan.c"

if [ -z "$(which gcc)" ] || [ -z "$(which make)" ]; then
	sudo apt-get -qqy update && sudo apt-get install -qqy make gcc "linux-headers-$(uname -r)"
fi

make clean all

sudo cp -v vxcan.ko "/lib/modules/$(uname -r)/kernel/net/can/"
sudo depmod -A

sudo modprobe vxcan
sudo modprobe can-gw
