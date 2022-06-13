#!/usr/bin/env python3
# /********************************************************************************
# * Copyright (c) 2022 Contributors to the Eclipse Foundation
# *
# * See the NOTICE file(s) distributed with this work for additional
# * information regarding copyright ownership.
# *
# * This program and the accompanying materials are made available under the
# * terms of the Apache License 2.0 which is available at
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * SPDX-License-Identifier: Apache-2.0
# ********************************************************************************/

import getopt
import logging
import os
import sys

import grpc
import sdv.edge.comfort.hvac.v1.hvac_pb2 as pb2
import sdv.edge.comfort.hvac.v1.hvac_pb2_grpc as pb2_grpc
from sdv.edge.comfort.hvac.v1.hvac_pb2 import AcStatus

logger = logging.getLogger(__name__)


class HVACTestClient(object):
    """
    Client for gRPC functionality
    """

    def __init__(self, hvac_addr: str):
        self._hvac_addr = hvac_addr
        logger.info("Connecting to HVAC service %s", self._hvac_addr)

        # instantiate a channel
        self.channel = grpc.insecure_channel(self._hvac_addr)

        # bind the client and the server
        self.stub = pb2_grpc.HvacStub(self.channel)

    def execute_methods(self, ac_status: AcStatus, ac_temp: float) -> None:
        """
        Client function to call the rpc for HVACService methods
        """
        logger.info("Setting AC Status: %s", self.get_hvac_str(ac_status))
        request = pb2.SetAcStatusRequest(status=ac_status)
        self.stub.SetAcStatus(request)

        logger.info("Setting Temperature: %s", ac_temp)
        request = pb2.SetTemperatureRequest(temperature=ac_temp)
        self.stub.SetTemperature(request)

        logger.info("Done.")

    def get_hvac_str(self, hvac_value) -> str:
        if hvac_value == 0:
            return "AcStatus.OFF"
        elif hvac_value == 1:
            return "AcStatus.ON"
        else:
            return "Invalid value: {}".format(hvac_value)


def main(argv):
    """Main function"""

    default_addr = "127.0.0.1:50052"
    default_temp = "42.0"
    default_status = "1"

    _usage = (
        "Usage: ./testclient.py --addr <host:name>"  # shorten line
        " --temp=AC_TEMP --status=AC_STATUS\n\n"
        "Environment:\n"
        "  'VDB_ADDR'    Databroker address (host:port). Default: {}\n"
        "  'AC_TEMP'     Desired AC Temperature. Default: {}\n"
        "  'AC_STATUS'   AC Status (0=OFF, 1=ON). Default: {}\n".format(
            default_addr, default_temp, default_status
        )
    )

    # environment values (overridden by cmdargs)
    hvac_addr = os.getenv("HVAC_ADDR", default_addr)
    ac_temp = float(os.environ.get("AC_TEMP", default_temp))
    ac_status = AcStatus.ON if os.getenv("AC_STATUS") != "0" else AcStatus.OFF

    # parse cmdline args
    try:
        opts, args = getopt.getopt(argv, "ha:t:s:", ["addr=", "temp=", "status="])
        for opt, arg in opts:
            if opt == "-h":
                print(_usage)
                sys.exit(0)
            elif opt in ("-a", "--addr"):
                hvac_addr = arg
            elif opt in ("-t", "--temp"):
                ac_temp = float(arg)
            elif opt in ("-s", "--status"):
                ac_status = AcStatus.ON if arg != "0" else AcStatus.OFF
            else:
                print("Unknown arg: {}".format(opt))
                print(_usage)
                sys.exit(1)
    except getopt.GetoptError:
        print(_usage)
        sys.exit(1)

    client = HVACTestClient(hvac_addr)
    client.execute_methods(ac_status, ac_temp)


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    main(sys.argv[1:])
