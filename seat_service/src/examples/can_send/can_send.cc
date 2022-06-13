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
 * @file      can_send.cc
 * @brief     File contains 
 */

#include <thread>

#include "can_raw_socket.h"

/**
 * @brief main 
 */
int main(int argc, char* argv[]) {
    sdv::hal::CanRawSocket can_if("vcan0");

    sdv::hal::CanFrame frame = {
        .can_id = 0x712,
        .data = {0x1, 0x50, 20, 0, 0, 0, 0, 0},
    };

    for (auto i = 0; i < 500; i++) {
        switch (i) {
            case 40:
                frame.data[2] = 21;
                break;
            case 80:
                frame.data[2] = 22;
                break;
            case 120:
                frame.data[2] = 23;
                break;
            case 160:
                frame.data[2] = 24;
                break;
            case 200:
                frame.data[2] = 26;
                break;
            case 240:
                frame.data[2] = 28;
                break;
            case 280:
                frame.data[2] = 30;
                break;
            case 320:
                frame.data[2] = 32;
                break;
            case 360:
                frame.data[2] = 34;
                break;
        }
        frame.data[7] = i;
        can_if.SendFrame(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}