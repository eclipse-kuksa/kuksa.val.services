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
#include <thread>
#include <chrono>

#include "data_broker_feeder.h"
#include "create_datapoint.h"

const std::string seat_position_vss3 = "Vehicle.Cabin.Seat.Row1.Pos1.Position";
const std::string seat_position_vss4 = "Vehicle.Cabin.Seat.Row1.DriverSide.Position";

using sdv::databroker::v1::Datapoint;
using sdv::databroker::v1::DataType;
using sdv::databroker::v1::ChangeType;

/**
 * @brief main Instantiate the feeder. It requires a channel, out of which the actual RPCs
 * are created. This channel models a connection to an endpoint specified by
 * the argument "--target=".
 * We indicate that the channel isn't authenticated (use of InsecureChannelCredentials()).
 */
int main(int argc, char** argv) {

    std::string target_str = "localhost:55555";
    std::string arg_str("--target");
    std::string vss3_str("--vss3");
    bool use_vss3 = false;

    for (int i = 1; i < argc; i++) {
        std::string arg_val = argv[i];
        if (vss3_str == arg_val) {
            use_vss3 = true;
        } else {
            size_t start_pos = arg_val.find(arg_str);
            if (start_pos != std::string::npos) {
                start_pos += arg_str.size();
                if (arg_val[start_pos] == '=') {
                    target_str = arg_val.substr(start_pos + 1);
                } else {
                    std::cout << "Target argument syntax is --target=<ip>:<port>" << std::endl;
                    exit(1);
                }
            } else {
                std::cout << "Usage: " << argv[0] << " --target=<ip>:<port> --vss3" << std::endl;
                exit(1);
            }
        }
    }

    auto dp_name = use_vss3 ? seat_position_vss3 : seat_position_vss4;
    sdv::broker_feeder::DatapointConfiguration metadata {
        {   dp_name,
            DataType::UINT16,
            ChangeType::ON_CHANGE,
            sdv::broker_feeder::createDatapoint(0U),
            "dummy description"
        }
    };
    auto client = sdv::broker_feeder::KuksaClient::createInstance(target_str);
    auto feeder = sdv::broker_feeder::DataBrokerFeeder::createInstance(client, std::move(metadata));

    // Setup feeder thread
    std::cout << "### Starting DataBrokerFeeder on " << target_str << std::endl;
    std::thread feeder_thread(&sdv::broker_feeder::DataBrokerFeeder::Run, feeder);

    std::cout << "### waiting DataBrokerFeeder.Ready()..." << std::endl;
    for (auto i=0; i<10; i++) {
        if (feeder->Ready()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "### DataBrokerFeeder.Ready(): " << std::boolalpha << feeder->Ready() << std::endl;

    int timeout = std::stoi(sdv::utils::getEnvVar("TIMEOUT", "0"));
    int step = std::stoi(sdv::utils::getEnvVar("STEP", "10"));
    for (int i = 0; i <= 1000; i += step) {
        std::cout << "   Feed Value " << i << " to '" << dp_name << "'" << std::endl;
        sdv::databroker::v1::Datapoint datapoint;
        datapoint.set_uint32_value((uint32_t)i); // type should be UINT16
        feeder->FeedValue(dp_name, datapoint);
        // NOTE: feeding without any sleep causes feeder Run() thread starvation
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    }

    // feed bad value
    {
        auto val = "bad value";
        std::cout << "   Feed bad Value " << val << " to '" << dp_name << "'" << std::endl;
        sdv::databroker::v1::Datapoint datapoint;
        datapoint.set_string_value(val);
        feeder->FeedValue(dp_name, datapoint);
    }

    std::cout << "### waiting..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "### Shutting down feeder..." << std::endl;
    feeder->Shutdown();

    feeder_thread.join();

    return 0;
}
