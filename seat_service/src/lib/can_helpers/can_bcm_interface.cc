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
 * @file      can_bcm_interface.cc
 * @brief     File contains functions for send and receive can massages
 */
#include "can_bcm_interface.h"

#include <linux/can.h>
#include <linux/can/bcm.h>
#include <net/if.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <thread>
namespace sdv {
namespace hal {

/**
* @brief struct BcmMessageRaw for storing the can message
* @param msg_head
* @param frame 
*/
struct BcmMessageRaw {
    bcm_msg_head msg_head;
    can_frame frame;
};

/**
* @brief CanBcmInterface 
* @param if_name name of the can interface
*/
CanBcmInterface::CanBcmInterface(std::string if_name)
    : cb_([](BcmEventType, const CanFrame &) {}) {
    if ((socket_ = socket(PF_CAN, SOCK_DGRAM, CAN_BCM)) < 0) {
        perror("bcmsocket");
        return;
    }

    auto ifindex = if_nametoindex(if_name.c_str());
    if (!ifindex) {
        perror("if_nametoindex");
        return;
    }
    struct sockaddr_can caddr;
    memset(&caddr, 0, sizeof(caddr));
    caddr.can_family = PF_CAN;
    caddr.can_ifindex = ifindex;

    if (connect(socket_, (struct sockaddr *)&caddr, sizeof(caddr)) < 0) {
        perror("connect");
        return;
    }
}

/**
* @brief ~CanBcmInterface decontructor close open socket
* 
*/
CanBcmInterface::~CanBcmInterface() {
    if (socket_ >= 0) {
        close(socket_);
    }
}

/**
* @brief SendFrame 
* @param frame Can frame 
*/
bool CanBcmInterface::SendFrame(const CanFrame &frame) {
    BcmMessageRaw msg;

    memset(&msg, 0, sizeof(msg));
    msg.msg_head.nframes = 1;

    msg.msg_head.can_id = frame.can_id;
    msg.msg_head.opcode = TX_SEND;

    msg.frame.can_id = frame.can_id;
    msg.frame.can_dlc = frame.data.size();

    for (auto i = 0; i < frame.data.size(); i++) {
        msg.frame.data[i] = frame.data[i];
    }

    if (write(socket_, &msg, sizeof(msg)) < 0) {
        perror("send");
        return false;
    }
    return true;
}

/**
* @brief SetCallback 
* @param cb callback function 
*/
void CanBcmInterface::SetCallback(BcmCallback cb) { cb_ = cb; }

/**
 * @brief SubscribeCyclicChange
 * @param can_id can message id
 * @param data_mask
 * @param timeout
*/
void CanBcmInterface::SubscribeCyclicChange(uint32_t can_id, std::vector<uint8_t> data_mask,
                                            std::chrono::milliseconds timeout) {
    // Setup
    BcmMessageRaw msg;

    memset(&msg, 0, sizeof(msg));
    msg.msg_head.nframes = 1;

    msg.msg_head.can_id = can_id;
    msg.msg_head.opcode = RX_SETUP;
    msg.msg_head.flags = SETTIMER;

    if (timeout > std::chrono::microseconds(0)) {
        auto usecs = std::chrono::duration_cast<std::chrono::microseconds>(timeout).count();
        msg.msg_head.ival1 = {
            .tv_sec = usecs / 1000000000,
            .tv_usec = usecs % 1000000000,
        };
    }

    msg.frame.can_id = can_id;
    msg.frame.can_dlc = data_mask.size();

    for (auto i = 0; i < data_mask.size(); i++) {
        msg.frame.data[i] = data_mask[i];
    }

    if (write(socket_, &msg, sizeof(msg)) < 0) {
        perror("send");
    }

    // Read back setup (verify it matches)
    // memset(&msg, 0, sizeof(msg));
    // msg.msg_head.nframes = 1;

    // msg.msg_head.can_id = can_id;
    // msg.msg_head.opcode = RX_READ;

    // if (write(socket_, &msg, sizeof(msg)) < 0 ) {
    //     perror("send");
    // }
}

/**
 * @brief RunForever
 * 
*/
void CanBcmInterface::RunForever() {
    BcmMessageRaw msg;

    /** 
     * Read
     */
    while (1) {
        auto nbytes = read(socket_, &msg, sizeof(msg));
        if (nbytes < 0) {
            if (errno != EAGAIN) {
                perror("read");
            }
            CanFrame frame;
            cb_(BcmEventType::ERROR, frame);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else {
            BcmEventType event_type;
            CanFrame frame;

            frame.can_id = msg.frame.can_id;
            switch (msg.msg_head.opcode) {
                case RX_CHANGED:
                    event_type = BcmEventType::DATA_CHANGED;
                    break;
                case RX_TIMEOUT:
                    event_type = BcmEventType::DATA_TIMEOUT;
                    break;
                default:
                    continue;
            }
            frame.can_id = msg.frame.can_id;
            if (msg.frame.can_dlc > 0) {
                frame.data = {msg.frame.data, msg.frame.data + msg.frame.can_dlc};
            }

            cb_(event_type, frame);
        }
    }
}

}  // namespace hal
}  // namespace sdv
