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

INSTALL="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Used can interface, if hw is missing set it to "vcan0" and run:
#   ~/vehicle_hal/tools/setup-vcan; ~/vehicle_hal/tools/sim-SECU1_STAT vcan0 0
# or dedicated can-sumulator service
[ -z "$CAN" ] && CAN="can0"
### special can value for runnig SeatAdjuster engine on a faked sockedCAN for specified process
#CAN="cansim"

sig_handler() {
	echo "[$0][sig_handler] Terminating..."
	CAN_WAIT=0
	# FIXME: killall not in docker image..
	#killall ecu-reset

	ps -ef
	# kill child processes with script's pid
	pkill -e -P $$

	exit 42
}

trap sig_handler SIGTERM SIGINT

### Wait for can device available
if [ -n "$CAN_WAIT" ] && [ "$CAN" != "cansim" ]; then
	echo "[$0] Waiting for $CAN ($CAN_WAIT s)..."
	# avoid ip tool dependency
	sec=0
	while [ ! -e /sys/class/net/$CAN ]; do
		sleep 1
		sec=$((sec + 1))
		if [ $sec -ge $CAN_WAIT ]; then
			echo "[$0] Timedout waiting for: /sys/class/net/$CAN"
			exit 3
		fi
	done
fi

### Seat Controller Configuration ###
# 1=enable raw can dumps, too verbose - only for troubleshooting
[ -z "$SC_RAW" ] && export SC_RAW=0

# Seat adjuster move operation timeout (ms). After it is reached motors are stopped
[ -z "$SC_TIMEOUT" ] && export SC_TIMEOUT=15000

# 1=dump SECU1_STAT can frames (useful to check for unmanaged seat position changes)
[ -z "$SC_STAT" ] && export SC_STAT=0

# 1=dump Coontrol Loop messages. Only dumps when active set position operation is running.
[ -z "$SC_CTL" ] && export SC_CTL=1

# Seat position moror RPM/100. e.g. 80=8000rpm. Suggested range [30..120]
[ -z "$SC_RPM" ] && export SC_RPM=80

# SeatAdjuster c++ debug
[ -z "$SA_DEBUG" ] && export SA_DEBUG=0

# 1=Exit process on CAN I/O or other fatal error
[ -z "$SA_EXIT" ] && export SA_EXIT=1
# attempt to do grpc cleanup on exit (may hang the exit!)
#export GRPC_CLEANUP=1

### DataFeeder configuration ###

# DataFeeder debug (=2 for verbose)
[ -z "$DBF_DEBUG" ] && DBF_DEBUG=0
# If needed to set 'dapr-app-id'
#VEHICLEDATABROKER_DAPR_APP_ID=

### Seat Service properties ###
[ -z "$SEAT_DEBUG" ] && SEAT_DEBUG=1

# uncomment for grpc troubleshooting
#export GRPC_VERBOSITY="INFO"
#export GRPC_TRACE="api" #channel,executor,op_failure

# Allow reconfiguration of Seat Service host:port via env (docker)
[ -z "$SERVICE_HOST" ] && SERVICE_HOST="0.0.0.0"
[ -z "$SERVICE_PORT" ] && SERVICE_PORT=50051

### Calibrate motor before service start (needs to be done after ECU reset, or not in learned state )
if [ "$CAN" = "can0" ]; then
	echo "### Calibrating ECU on $CAN"
	# timeout 30s, usually it takes ~25s for calibration
	"$INSTALL/tools/ecu-reset" -s "$CAN"
fi

if [ "$CAN" = "cansim" ]; then
	echo "### Starting VAL Seat Service [$SERVICE_HOST:$SERVICE_PORT] on Simulated SocketCAN!"
	exec stdbuf --output=L $INSTALL/tools/cansim $INSTALL/seat_service "$CAN" "$SERVICE_HOST" $SERVICE_PORT
else
	echo "### Starting VAL Seat Service [$SERVICE_HOST:$SERVICE_PORT] on $CAN..."
	exec stdbuf --output=L $INSTALL/seat_service "$CAN" "$SERVICE_HOST" $SERVICE_PORT
fi
