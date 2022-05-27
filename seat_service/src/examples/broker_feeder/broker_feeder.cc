/********************************************************************************
* Copyright (c) 2022 Contributors to the Eclipse Foundation
*
* See the NOTICE file(s) distributed with this work for additional
* information regarding copyright ownership.
*
* This program and the accompanying materials are made available under the
* terms of the Apache License 2.0 which is available at
* http://www.apache.org/licenses/LICENSE-2.0
*
* SPDX-License-Identifier: Apache-2.0
********************************************************************************/
/**
 * @file      broker_feeder.cc
 * @brief     File contains
 */

#include <iostream>
#include <string>

#include "data_broker_feeder.h"


/**
 * @brief main Instantiate the feeder. It requires a channel, out of which the actual RPCs
 * are created. This channel models a connection to an endpoint specified by
 * the argument "--target=".
 * We indicate that the channel isn't authenticated (use of InsecureChannelCredentials()).
 */
int main(int argc, char** argv) {

    std::string target_str = "localhost:55555";
    std::string arg_str("--target");

    for (int i = 1; i < argc; i++) {
        std::string arg_val = argv[i];
        size_t start_pos = arg_val.find(arg_str);
        if (start_pos != std::string::npos) {
            start_pos += arg_str.size();
            if (arg_val[start_pos] == '=') {
                target_str = arg_val.substr(start_pos + 1);
            } else {
                std::cout << "Target argument syntax is --target=<ip>:<port>" << std::endl;
                return 1;
            }
        }
    }

    auto feeder = sdv::broker_feeder::DataBrokerFeeder::createInstance(target_str, {});

    for (int i = 0; i < 1000; i += 10) {
        std::cout << "Feed Value "<< i <<" to 'Vehicle.Cabin.Seat.Row1.Pos1.Position'" << std::endl;
        sdv::databroker::v1::Datapoint datapoint;
        datapoint.set_int32_value(i);
        feeder->FeedValue("Vehicle.Cabin.Seat.Row1.Pos1.Position", datapoint);
    }

    return 0;
}
