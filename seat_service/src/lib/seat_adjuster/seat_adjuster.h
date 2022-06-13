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
 * @file      seat_adjuster.h
 * @brief         
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

namespace sdv {

/**
 * @brief Constant for indicating unavailable position in GetSeatPosition()
 */
#define SEAT_POSITION_INVALID   -1

enum class SetResult {
    OK = 0,
    /** Generic error */
    UNSPECIFIC_ERROR = 1,
    /* SocketCAN not available */
    NO_CAN = 2,
    /* CAN interface index error */
    CAN_IF_INDEX_ERROR = 3,
    /* SocketCAN bind() error */
    CAN_BIND_ERROR = 4,
    /* SocketCAN i/o error */
    CAN_IO_ERROR = 5,
    /** Invalid argument(s) */
    INVALID_ARG = 6,
    /** Can signals not coming from ECU */
    NO_FRAMES = 7,
};

/**
 * @brief SeatAdjuster
 * 
 */
class SeatAdjuster {
public:
    static std::shared_ptr<SeatAdjuster> createInstance(const std::string& can_if_name);
    virtual ~SeatAdjuster() = default;

    virtual int GetSeatPosition() = 0;
    virtual SetResult SetSeatPosition(int position_in_percent) = 0;
    virtual void SubscribePosition(std::function<void(int)> cb) = 0;

protected:
    SeatAdjuster() = default;
    SeatAdjuster(const SeatAdjuster&) = delete;
    SeatAdjuster& operator=(const SeatAdjuster&) = delete;
};

}  // namespace sdv
