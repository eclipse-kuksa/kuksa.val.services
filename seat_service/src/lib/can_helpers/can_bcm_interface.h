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
 * @file      can_bcm_interface.h
 * @brief     File contains 
 */
#pragma once

#include <stdint.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "can_raw_socket.h"

namespace sdv {
namespace hal {

/**
 * @brief     enum BcmEventType
 * @param DATA_CHANGED BCM message with updated CAN frame (detected content change).
 * Sent on first message received or on receipt of revised CAN messages.
 * @param DATA_TIMEOUT Cyclic message is detected to be absent (timer expired).
 * @param ERROR General Error
 */
enum class BcmEventType {
    DATA_CHANGED,                     
    DATA_TIMEOUT,  
    ERROR,         
};

/**
 * @brief     struct BcmMessage
 * @param event_type 
 * @param can_frame 
 */
struct BcmMessage {
    BcmEventType event_type;
    CanFrame can_frame;
};
/**
 * @brief prototype BcmCallback 
 */
typedef std::function<void(BcmEventType, const CanFrame&)> BcmCallback;

/**
 * @brief CanBcmInterface used for send and receive can messsages.
 */
class CanBcmInterface {
   public:
    CanBcmInterface(std::string if_name);
    ~CanBcmInterface();
    /**
     * non construction-copyable
     */
    CanBcmInterface(const CanBcmInterface&) = delete;
    /**
     * non copyable
     */
    CanBcmInterface& operator=(const CanBcmInterface&) = delete;  

    bool SendFrame(const CanFrame& frame);
    void SubscribeCyclicChange(uint32_t can_id, std::vector<uint8_t> data_mask, std::chrono::milliseconds timeout);
    void SetCallback(BcmCallback cb);

    void RunForever();
    void Stop();

   private:
    int socket_;
    BcmCallback cb_;
};

}  // namespace hal
}  // namespace sdv
