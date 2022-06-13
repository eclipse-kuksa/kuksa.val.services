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
 * @file      seat_adjuster.cc
 * @brief         
 * 
 */

#include "seat_adjuster.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "seat_controller.h"

namespace sdv {

// Module Logging
#define MODULE "SeatAdjuster"
#define LOG_FN std::string("[" MODULE "::").append(__func__).append("] ")

static int debug = ::getenv("SA_DEBUG") && atoi(::getenv("SA_DEBUG"));
static int exit_on_error = ::getenv("SA_EXIT") && ::atoi(::getenv("SA_EXIT"));

// FIXME: try stopping the service gracefully before exit, make it a common function
void abort_service(int rc) {
    // Cleanup seatctrl context, stops CTL thread, socket cleanup.
    std::cerr << LOG_FN << "*** Aborting service:(" << rc << ") ***" << std::endl;
    // TODO: cleanup for grpc
    exit(rc);
}

class SeatAdjusterImpl:
    public SeatAdjuster {
public:
    SeatAdjusterImpl(const std::string& can_if_name);
    ~SeatAdjusterImpl() override;

    int GetSeatPosition() override;
    SetResult SetSeatPosition(int positionInPercent) override;

    void SubscribePosition(std::function<void(int)> cb) override {
        if (debug) std::cerr << LOG_FN << "setting callback: " << cb.target_type().name() << std::endl;
        cb_ = cb;
    }

private:
    seatctrl_context_t ctx_;
    std::string can_if_name_;
    std::function<void(int)> cb_;
    static void seatctrl_event_cb(SeatCtrlEvent event, int value, void* user_data);
};


std::shared_ptr<SeatAdjuster> SeatAdjuster::createInstance(const std::string& can_if_name) {
    return std::make_shared<SeatAdjusterImpl>(can_if_name);
}


/**
 * @brief Initializes provided seatctrl_config_t* with default values (supports overriding via getenv())
 * Initialize seat_controller context with specified seat_controller config
 * opens socket can, starts CTL thread.
 */
SeatAdjusterImpl::SeatAdjusterImpl(const std::string& can_if_name)
    : can_if_name_{can_if_name}
    , cb_{nullptr}
{
    error_t rc;
    // init
    seatctrl_config_t config;

    const std::string prefix = LOG_FN;

    if (debug) {
        std::cerr << prefix << "Using: " << can_if_name_ << ", exit_on_error: " << exit_on_error << std::endl;
    }

    // Initializes provided seatctrl_config_t* with default values (supports overriding via getenv())
    rc = seatctrl_default_config(&config);
    config.can_device = can_if_name_.c_str();

    // Initialize seatctrl context with specified seatctrl config
    rc = seatctrl_init_ctx(&ctx_, &config);
    if (rc != SEAT_CTRL_OK) {
        std::cerr << prefix << "seatctrl_init_ctx() failed!" << std::endl;
        if (exit_on_error) abort_service(rc);
        return;
    }

    rc = seatctrl_set_event_callback(&ctx_, seatctrl_event_cb, this);
    if (rc != SEAT_CTRL_OK) {
        std::cerr << prefix << "seatctrl_set_event_callback() failed!" << std::endl;
        if (exit_on_error) abort_service(rc);
        return;
    }

    // opens socket can, starts CTL thread.
    rc = seatctrl_open(&ctx_);
    if (rc != SEAT_CTRL_OK) {
        std::cerr << prefix << "seatctrl_open() failed!" << std::endl;
        if (exit_on_error) abort_service(rc);
    }
}

/**
 * @brief Cleanup seatctrl context, stops CTL thread, socket cleanup.
 * 
 */
SeatAdjusterImpl::~SeatAdjusterImpl() {
    // Cleanup seatctrl context, stops CTL thread, socket cleanup.
    std::cerr << LOG_FN << "cleaning up..." << std::endl;
    error_t rc = seatctrl_close(&ctx_);
}

/**
 * @brief Gets last received seat position (motor1).
 * 
 * @return int seat position in percents [0..100] or SEAT_POSITION_INVALID(-1) if position not known/available
 */
int SeatAdjusterImpl::GetSeatPosition() {
    // Gets last repoted seat position, value 255=invalid
    auto pos = seatctrl_get_position(&ctx_);
    if (pos == MOTOR_POS_INVALID || pos < 0) { // (pos < 0) -> SEAT_CTRL_ERR_XXX
        pos = SEAT_POSITION_INVALID;  // considered as invalid value
    }
    if (debug) {
        std::cerr << LOG_FN << "-> " << pos << std::endl;
    }
    return pos;
}

/**
 * @brief Set absolute Seat position (%) asynchronously.
 * 
 * @param positionInPercent position [0..100]
 * @return SetResult SetResult::OK if seat movement has started or mapped error from seatctrl_set_position()
 */
SetResult SeatAdjusterImpl::SetSeatPosition(int positionInPercent) {
    std::cerr << LOG_FN << "setting seat position to " << positionInPercent << "%" << std::endl;
    error_t rc = seatctrl_set_position(&ctx_, positionInPercent);
    if (rc == SEAT_CTRL_OK) {
        return SetResult::OK;
    }
    std::cerr << LOG_FN << "setting seat position failed: " << rc << std::endl;
    switch (rc) {
    case SEAT_CTRL_ERR:
        return SetResult::UNSPECIFIC_ERROR;
    case SEAT_CTRL_ERR_NO_CAN:
        return SetResult::NO_CAN;
    case SEAT_CTRL_ERR_IFR:
        return SetResult::CAN_IF_INDEX_ERROR;
    case SEAT_CTRL_ERR_CAN_BIND:
        return SetResult::CAN_BIND_ERROR;
    case SEAT_CTRL_ERR_CAN_IO:
        return SetResult::CAN_IO_ERROR;
    case SEAT_CTRL_ERR_INVALID:
        return SetResult::INVALID_ARG;
    case SEAT_CTRL_ERR_NO_FRAMES:
        return SetResult::NO_FRAMES;
    default:
        return SetResult::UNSPECIFIC_ERROR;
    }
}



/**
 * @brief Helper function for use as callback function in C code
 */
void SeatAdjusterImpl::seatctrl_event_cb(SeatCtrlEvent event, int value, void* user_data) {
    const std::string prefix = LOG_FN;
    static bool cb_null_dumped = false;  // prevent periodic null warnings on each cb call.

    if (event == SeatCtrlEvent::CanError) {
        std::cerr << prefix << "*** CAN error detected: " << value << std::endl;
        if (exit_on_error) abort_service(value);
        return;
    }

    if (event == SeatCtrlEvent::Motor1Pos) {
        if (user_data != nullptr) {
            SeatAdjusterImpl* seat_adjuster = static_cast<SeatAdjusterImpl*>(user_data);
            if (seat_adjuster->cb_ != nullptr) {
                if (debug > 1) { // consider this verbose
                    std::cerr << prefix << "calling *" << seat_adjuster->cb_.target_type().name() << "(" << value << ")"
                            << std::endl;
                }
                // adjust scaling for value to match GetSeatPosition()
                int pos = (value == MOTOR_POS_INVALID) ? -1 : value;
                seat_adjuster->cb_(pos);
                cb_null_dumped = false;
            } else {
                if (!cb_null_dumped) {
                    std::cerr << prefix << "cb_ is NULL!" << std::endl;
                }
                cb_null_dumped = true;
            }
        } else {
            if (debug) {
                std::cerr << prefix << "user_data is NULL!" << std::endl;
            }
        }
    }
}


}  // namespace sdv
