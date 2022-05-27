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
 * @file      can_raw_socket.cc
 * @brief     File contains functions for can raw socket
 */

#include <iostream>

#include "can_raw_socket.h"
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace sdv {
namespace hal {

/**
 * @brief CanRawSocket
 * 
 */
CanRawSocket::CanRawSocket(std::string name) {
    if ((socket_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        ::perror("socket");
        return;
    }

    struct ifreq ifr;
    ::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
    if (!ifr.ifr_ifindex) {
        perror("if_nametoindex");
        return;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return;
    }
}

/**
 * @brief SendFrame
 * 
 */
bool CanRawSocket::SendFrame(const CanFrame &frame) {
    can_frame raw_frame;
    uint8_t fixed_length = 8;

    auto raw_frame_size = sizeof(raw_frame);
    memset(&raw_frame, 0, raw_frame_size);
    raw_frame.can_id = frame.can_id;
    raw_frame.can_dlc = frame.data.size();

    for (auto i = 0; i < frame.data.size(); i++) {
        raw_frame.data[i] = frame.data[i];
    }

    /**
     *  send frame 
     */
    if (write(socket_, &raw_frame, raw_frame_size) != raw_frame_size) {
        perror("write");
        return false;
    }

    return true;
}

/**
 * @brief
 * 
 */
CanRawSocket::~CanRawSocket() {
    if (socket_ >= 0) {
        ::close(socket_);
    }
}

}  // namespace hal
}  // namespace sdv
