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
 * @file      can_raw_socket.h
 * @brief     File contains functions for can raw socket
 */

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace sdv {
namespace hal {

/**
 * @brief CanFrame
 * 
 */
struct CanFrame {
    uint32_t can_id;            // 11 bits
    std::vector<uint8_t> data;  // 0 - 8 bytes
};

/**
 * @brief CanRawSocket
 * 
 */
class CanRawSocket {
   public:
    CanRawSocket(std::string if_name);
    ~CanRawSocket();
    /**
     * non construction-copyable
     */
    CanRawSocket(const CanRawSocket&) = delete;
    /**
     * non copyable
     */            
    CanRawSocket& operator=(const CanRawSocket&) = delete;  

    bool SendFrame(const CanFrame& frame);

   private:
    int socket_;
};

}  // namespace hal
}  // namespace sdv
