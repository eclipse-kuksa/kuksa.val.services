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
 * @file      can_subscribe.cc
 * @brief     File contains 
 */

#include <iostream>

#include "can_bcm_interface.h"

/**
 * @brief main 
 */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <CAN_IF_NAME>" << std::endl;
        return 1;
    }

    std::string can_if_name(argv[1]);
    sdv::hal::CanBcmInterface bcm(can_if_name);

    bcm.SetCallback([](sdv::hal::BcmEventType event_type, const sdv::hal::CanFrame& msg) {
        switch (event_type) {
            case sdv::hal::BcmEventType::DATA_CHANGED: {
                /**
                 *  CAN ID
                 */
                std::cerr << std::hex << std::showbase << msg.can_id << " ";

                /**
                 *  Byte 2 (motor position in %)
                 */
                std::cerr << std::dec << (int)msg.data[2] << "%" << std::endl;
                break;
            }
            case sdv::hal::BcmEventType::DATA_TIMEOUT:
                std::cerr << "No data received" << std::endl;
                break;
            case sdv::hal::BcmEventType::ERROR:
                std::cerr << "An error occurred" << std::endl;
                break;
        }
    });
    sdv::hal::CanFrame frame = {.can_id = 0x705, .data = {0x1, 0x50, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}};
    bcm.SendFrame(frame);
    bcm.SubscribeCyclicChange(0x712, {0, 0, 0xff}, std::chrono::milliseconds(0));
    bcm.RunForever();
}