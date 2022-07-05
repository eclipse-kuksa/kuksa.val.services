#!/usr/bin/env python

########################################################################
# Copyright (c) 2020 Robert Bosch GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
########################################################################

import argparse
import configparser
import logging
import os
import queue
import json
import sys
import time
from signal import SIGINT, SIGTERM, signal

import canplayer
import dbc2vssmapper
import dbcreader
import grpc
import j1939reader

# kuksa related
from kuksa_viss_client import KuksaClientThread
# databroker related
import databroker

# global variable for usecase, default databroker
USE_CASE = ""

log = logging.getLogger("dbcfeeder")

def init_logging(loglevel):
    # create console handler and set level to debug
    console_logger = logging.StreamHandler()
    console_logger.setLevel(logging.DEBUG)

    # create formatter
    if sys.stdout.isatty():
        formatter = ColorFormatter()
    else:
        formatter = logging.Formatter(
            fmt="%(asctime)s %(levelname)s %(name)s: %(message)s"
        )

    # add formatter to console_logger
    console_logger.setFormatter(formatter)

    # add console_logger as a global handler
    root_logger = logging.getLogger()
    root_logger.setLevel(loglevel)
    root_logger.addHandler(console_logger)


class ColorFormatter(logging.Formatter):
    FORMAT = "{time} {{loglevel}} {logger} {msg}".format(
        time="\x1b[2m%(asctime)s\x1b[0m",  # grey
        logger="\x1b[2m%(name)s:\x1b[0m",  # grey
        msg="%(message)s",
    )
    FORMATS = {
        logging.DEBUG: FORMAT.format(loglevel="\x1b[34mDEBUG\x1b[0m"),  # blue
        logging.INFO: FORMAT.format(loglevel="\x1b[32mINFO\x1b[0m"),  # green
        logging.WARNING: FORMAT.format(loglevel="\x1b[33mWARNING\x1b[0m"),  # yellow
        logging.ERROR: FORMAT.format(loglevel="\x1b[31mERROR\x1b[0m"),  # red
        logging.CRITICAL: FORMAT.format(loglevel="\x1b[31mCRITICAL\x1b[0m"),  # red
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


class Feeder:
    def __init__(self):
        self._shutdown = False
        self._reader = None
        self._player = None
        self._mapper = None
        self._provider = None
        self._connected = False
        self._registered = False
        self._can_queue = queue.Queue()

    def start(
        self,
        databroker_address,
        canport,
        dbcfile,
        mappingfile,
        candumpfile=None,
        use_j1939=False,
        grpc_metadata=None,
    ):
        log.debug("Use mapping: {}".format(mappingfile))
        self._mapper = dbc2vssmapper.mapper(mappingfile)

        if use_j1939:
            log.info("Use J1939 reader")
            self._reader = j1939reader.J1939Reader(
                rxqueue=self._can_queue,
                dbcfile=dbcfile,
                mapper=self._mapper,
            )
        else:
            log.info("Use DBC reader")
            self._reader = dbcreader.DBCReader(
                rxqueue=self._can_queue, dbcfile=dbcfile, mapper=self._mapper
            )

        if candumpfile:
            # use dumpfile
            log.info(
                "Using virtual bus to replay CAN messages (channel: %s)",
                canport,
            )
            self._player = canplayer.CANplayer(dumpfile=candumpfile)
            self._reader.start_listening(
                bustype="virtual", channel=canport, bitrate=500000
            )
            self._player.start_replaying(canport=canport)
        else:
            # use socketCAN
            log.info("Using socket CAN device '%s'", canport)
            self._reader.start_listening(bustype="socketcan", channel=canport)
       
        # databroker related
        if USE_CASE=="databroker":
            log.info("Connecting to Data Broker using %s", databroker_address)
            channel = grpc.insecure_channel(databroker_address)
            channel.subscribe(
                lambda connectivity: self.on_broker_connectivity_change(connectivity),
                try_to_connect=False,
            )
            self._provider = databroker.Provider(channel, grpc_metadata)
        self._run()

    def stop(self):
        log.info("Shutting down...")
        self._shutdown = True
        # Tell others to stop
        if self._reader is not None:
            self._reader.stop()
        if self._player is not None:
            self._player.stop()

    def is_stopping(self):
        return self._shutdown

    def on_broker_connectivity_change(self, connectivity):
        log.debug("Connectivity changed to: %s", connectivity)
        if (
            connectivity == grpc.ChannelConnectivity.READY or
            connectivity == grpc.ChannelConnectivity.IDLE
        ):
            # Can change between READY and IDLE. Only act if coming from
            # unconnected state
            if not self._connected:
                log.info("Connected to data broker")
                try:
                    self._register_datapoints()
                    self._registered = True
                except Exception:
                    log.error("Failed to register datapoints", exc_info=True)
                self._connected = True
        else:
            if self._connected:
                log.info("Disconnected from data broker")
            else:
                if connectivity == grpc.ChannelConnectivity.CONNECTING:
                    log.info("Trying to connect to data broker")
            self._connected = False
            self._registered = False

    def _register_datapoints(self):
        log.info("Register datapoints")
        for entry in self._mapper.mapping:
            if len(self._mapper.mapping[entry]["targets"]) != 1:
                log.warning(
                    "Singal %s has multiple targets: %s",
                    entry,
                    list(self._mapper.mapping[entry]["targets"].keys()),
                )
            self._provider.register(
                next(iter(self._mapper.mapping[entry]["targets"])),
                self._mapper.mapping[entry]["databroker"]["datatype"],
                self._mapper.mapping[entry]["databroker"]["changetype"],
                self._mapper.mapping[entry]["vss"]["description"],
            )
        
    def _run(self):
        # kuksa related
        if USE_CASE=="kuksa":
            global kuksaconfig
            kuksa = KuksaClientThread(kuksaconfig)
            kuksa.start()
            kuksa.authorize()
        
        while self._shutdown is False:
            # databroker related
            if USE_CASE=="databroker":
                if not self._connected:
                    time.sleep(0.2)
                    continue
                elif not self._registered:
                    time.sleep(1)
                    try:
                        self._register_datapoints()
                        self._registered = True
                    except Exception:
                        log.error("Failed to register datapoints", exc_info=True)
                        continue
            try:
                can_signal, can_value = self._can_queue.get(timeout=1)
                for target in self._mapper[can_signal]["targets"]:
                    value = self._mapper.transform(can_signal, target, can_value)
                    if value != can_value:
                        log.debug(
                            "  transform({}, {}, {}) -> {}".format(
                                can_signal, target, can_value, value
                            )
                        )
                    # None indicates the transform decided to not set the value
                    if value is None:
                        log.warning(
                            "failed to transform({}, {}, {})".format(
                                can_signal, target, can_value
                            )
                        )
                    else:
                        # get values out of the canreplay and map to desired signals
                        log.debug("Updating DataPoint(%s, %s)", target, value)
                        # databroker related
                        if USE_CASE=="databroker":
                            # kuksa needs "false" and databroker False
                            if value == "false":
                                value = False
                            # kuksa needs "true" and databroker True
                            elif value == "true":
                                value = True
                            else:
                                pass
                            self._provider.update_datapoint(target, value)
                        # kuksa related
                        elif USE_CASE=="kuksa":
                            resp=json.loads(kuksa.setValue(target, str(value)))
                            if "error" in resp:
                                if "message" in resp["error"]: 
                                   log.error("Error setting {}: {}".format(target, resp["error"]["message"]))
                                else:
                                   log.error("Unknown error setting {}: {}".format(target, resp))
                        else:
                            log.error("USE_CASE is not set to databroker or kuksa", exc_info=True)

            except grpc.RpcError:
                log.error("Failed to update datapoints", exc_info=True)
            except queue.Empty:
                pass
            except Exception:
                log.error("Exception caught in main loop", exc_info=True)


def parse_config(filename):
    configfile = None

    if filename:
        if not os.path.exists(filename):
            log.warning("Couldn't find config file {}".format(filename))
            raise Exception("Couldn't find config file {}".format(filename))
        configfile = filename
    else:
        config_candidates = [
            "/config/dbc_feeder.ini",
            "/etc/dbc_feeder.ini",
            "config/dbc_feeder.ini",
        ]
        for candidate in config_candidates:
            if os.path.isfile(candidate):
                configfile = candidate
                break

    log.info("Using config: {}".format(configfile))
    if configfile is None:
        return {}

    config = configparser.ConfigParser()
    readed = config.read(configfile)
    if log.level >= logging.DEBUG:
        log.debug(
            "# config.read({}):\n{}".format(
                readed,
                {section: dict(config[section]) for section in config.sections()},
            )
        )

    return config

def main(argv):
    # argument support
    parser = argparse.ArgumentParser(description="dbcfeeder")
    parser.add_argument("--config", metavar="FILE", help="Configuration file")
    parser.add_argument(
        "--dbcfile", metavar="FILE", help="DBC file used for parsing CAN traffic"
    )
    parser.add_argument(
        "--dumpfile", metavar="FILE", help="Replay recorded CAN traffic from dumpfile"
    )
    parser.add_argument("--canport", metavar="DEVICE", help="Read from this CAN device")
    parser.add_argument("--use-j1939", action="store_true", help="Use J1939")

    parser.add_argument(
        "--use-socketcan",
        action="store_true",
        help="Use SocketCAN (overriding any use of --dumpfile)",
    )
    parser.add_argument("--address", metavar="ADDR", help="Address of databroker")
    parser.add_argument(
        "--mapping",
        metavar="FILE",
        help="Mapping file used to map CAN signals to databroker datapoints",
    )
    parser.add_argument("--usecase", metavar="UC", help="Switch between kuksa and databroker usecase")

    args = parser.parse_args()

    config = parse_config(args.config)
    
    if args.usecase:
        usecase = args.usecase
    elif os.environ.get("USECASE"):
        usecase = os.environ.get("USECASE")
    elif "general" in config and "usecase" in config["general"]:
        usecase = config["general"]["usecase"]
    else:
        usecase = "databroker"
                  
    global USE_CASE 
    USE_CASE = usecase

    if USE_CASE=="databroker" or USE_CASE=="kuksa":
        log.info("Usecase: %s", USE_CASE)
    else:
        raise Exception("Invalid USECASE value: {}".format(USE_CASE))

    # kuksa related
    if USE_CASE=="kuksa":
        global kuksaconfig
        kuksaconfig = config
        if "kuksa_val" in config:  
            kuksaconfig = config["kuksa_val"]

    if args.address:
        databroker_address = args.address
    elif os.environ.get("DAPR_GRPC_PORT"):
        databroker_address = "127.0.0.1:{}".format(os.environ.get("DAPR_GRPC_PORT"))
    elif os.environ.get("VDB_ADDRESS"):
        databroker_address = os.environ.get("VDB_ADDRESS")
    elif "databroker" in config and "address" in config["databroker"]:
        databroker_address = config["databroker"]["address"]
    else:
        databroker_address = "127.0.0.1:55555"  # default

    if args.mapping:
        mappingfile = args.mapping
    elif os.environ.get("MAPPING_FILE"):
        mappingfile = os.environ.get("MAPPING_FILE")
    elif "general" in config and "mapping" in config["general"]:
        mappingfile = config["general"]["mapping"]
    else:
        mappingfile = "mapping.yml"

    if args.canport:
        canport = args.canport
    elif os.environ.get("CAN_PORT"):
        canport = os.environ.get("CAN_PORT")
    elif "can" in config and "port" in config["can"]:
        canport = config["can"]["port"]
    else:
        parser.print_help()
        print("ERROR:\nNo CAN port specified")
        return -1

    if os.environ.get("USE_J1939"):
        use_j1939 = os.environ.get("USE_J1939") == "1"
    elif "can" in config:
        use_j1939 = config["can"].getboolean("j1939", False)
    else:
        use_j1939 = False

    if args.dbcfile:
        dbcfile = args.dbcfile
    elif os.environ.get("DBC_FILE"):
        dbcfile = os.environ.get("DBC_FILE")
    elif "can" in config and "dbcfile" in config["can"]:
        dbcfile = config["can"]["dbcfile"]
    else:
        dbcfile = None

    if not dbcfile and not use_j1939:
        parser.print_help()
        print("\nERROR:\nNeither DBC file nor the use of J1939 specified")
        return -1

    candumpfile = None
    if not args.use_socketcan:
        if args.dumpfile:
            candumpfile = args.dumpfile
        elif os.environ.get("CANDUMP_FILE"):
            candumpfile = os.environ.get("CANDUMP_FILE")
        elif "can" in config and "candumpfile" in config["can"]:
            candumpfile = config["can"]["candumpfile"]

    if os.environ.get("VEHICLEDATABROKER_DAPR_APP_ID"):
        grpc_metadata = (
            ("dapr-app-id", os.environ.get("VEHICLEDATABROKER_DAPR_APP_ID")),
        )
    else:
        grpc_metadata = None

    feeder = Feeder()

    def signal_handler(signal_received, frame):
        log.info(f"Received signal {signal_received}, stopping...")

        # If we get told to shutdown a second time. Just do it.
        if feeder.is_stopping():
            log.warning("Shutdown now!")
            sys.exit(-1)

        feeder.stop()

    signal(SIGINT, signal_handler)
    signal(SIGTERM, signal_handler)

    log.info("Starting CAN feeder")
    feeder.start(
        databroker_address=databroker_address,
        canport=canport,
        dbcfile=dbcfile,
        mappingfile=mappingfile,
        candumpfile=candumpfile,
        use_j1939=use_j1939,
        grpc_metadata=grpc_metadata,
    )

    return 0


def parse_env_log(env_log, default=logging.INFO):
    def parse_level(level, default=default):
        if type(level) is str:
            if level.lower() in [
                "debug",
                "info",
                "warn",
                "warning",
                "error",
                "critical",
            ]:
                return level.upper()
            else:
                raise Exception(f"could not parse '{level}' as a log level")
        return default

    loglevels = {}

    if env_log is not None:
        log_specs = env_log.split(",")
        for log_spec in log_specs:
            spec_parts = log_spec.split("=")
            if len(spec_parts) == 1:
                # This is a root level spec
                if "root" in loglevels:
                    raise Exception("multiple root loglevels specified")
                else:
                    loglevels["root"] = parse_level(spec_parts[0])
            if len(spec_parts) == 2:
                logger = spec_parts[0]
                level = spec_parts[1]
                loglevels[logger] = parse_level(level)

    if "root" not in loglevels:
        loglevels["root"] = default

    return loglevels


if __name__ == "__main__":
    # Example
    #
    # Set log level to debug
    #   LOG_LEVEL=debug ./dbcfeeder.py
    #
    # Set log level to INFO, but for dbcfeeder.broker set it to DEBUG
    #   LOG_LEVEL=info,dbcfeeder.broker_client=debug ./dbcfeeder.py
    #
    # Other available loggers:
    #   dbcfeeder
    #   dbcfeeder.broker_client
    #   databroker (useful for feeding values debug)
    #   dbcreader
    #   dbcmapper
    #   can
    #   j1939
    #

    loglevels = parse_env_log(os.environ.get("LOG_LEVEL"))

    # set root loglevel etc
    init_logging(loglevels["root"])

    # helper for debugging in vs code from project root
    # os.chdir(os.path.dirname(__file__))

    # set loglevels for other loggers
    for logger, level in loglevels.items():
        if logger != "root":
            logging.getLogger(logger).setLevel(level)

    sys.exit(main(sys.argv))
