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
 * @file      seat_data_feeder.cc
 * @brief     (See seat_data_feeder.h)
 *
 */
#include <iostream>
#include <memory>
#include <string>

#include "seat_data_feeder.h"

#include "create_datapoint.h"
#include "data_broker_feeder.h"
#include "seat_adjuster.h"

extern int debug;

namespace sdv {
namespace seat_service {

using sdv::databroker::v1::Datapoint;
using sdv::databroker::v1::DataType;
using sdv::databroker::v1::ChangeType;
using sdv::databroker::v1::Datapoint_Failure;

SeatDataFeeder::SeatDataFeeder(std::shared_ptr<SeatAdjuster> seat_adjuster, const std::string& broker_addr)
    : seat_adjuster_(seat_adjuster)
{
    /* Define datapoints (metadata) of seat service
     */
    std::string seat_pos_name = "Vehicle.Cabin.Seat.Row1.Pos1.Position";
    sdv::broker_feeder::DatapointConfiguration metadata = {
        {"Vehicle.Cabin.SeatRowCount",
            DataType::UINT32,
            ChangeType::STATIC,
            broker_feeder::createDatapoint(1U),
            "Number of rows of seats"},
        {"Vehicle.Cabin.Seat.Row1.PosCount",
            DataType::UINT32,
            ChangeType::STATIC,
            broker_feeder::createDatapoint(1U),
            "Number of seats in row 1"},
        {seat_pos_name,
            DataType::UINT32,
            ChangeType::ON_CHANGE,
            broker_feeder::createNotAvailableValue(),
            "Longitudinal position of overall seat"}
    };

    /* Init feeder
     */
    broker_feeder_ = sdv::broker_feeder::DataBrokerFeeder::createInstance(broker_addr, std::move(metadata));

    /* Internally subscribe to signals to be fed to broker
     */
    seat_adjuster_->SubscribePosition([this, seat_pos_name](int position_in_percent) {
        const std::string self = "[SeatSvc][SeatDataFeeder] ";
        if (debug > 1) { // require more verbose for extra dump
            std::cout << self << "got pos: " << position_in_percent << "%" << std::endl;
        }
        Datapoint datapoint;
        if (0 <= position_in_percent && position_in_percent <= 100) {
            datapoint.set_uint32_value(position_in_percent * 10); // scale up to [0..1000]
        } else
        if (position_in_percent == -1) { // -1 replaces MOTOR_POS_INVALID in SeatAdjusterImpl::seatctrl_event_cb()
            datapoint.set_failure_value(Datapoint_Failure::Datapoint_Failure_NOT_AVAILABLE);
        } else {
            // values > 100 are invalid
            datapoint.set_failure_value(Datapoint_Failure::Datapoint_Failure_INVALID_VALUE);
        }
        if (debug) {
            if (datapoint.has_failure_value()) {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_pos_name << ", failure:"
                    << Datapoint_Failure_Name(datapoint.failure_value()) << ")" << std::endl;
            } else
            if (datapoint.has_uint32_value()) {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_pos_name << ", uint32:" << datapoint.uint32_value() << ")" << std::endl;
            } else {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_pos_name << ", unknown)" << std::endl;
            }
        }
        broker_feeder_->FeedValue(seat_pos_name, datapoint);
    });
}
void SeatDataFeeder::Run() { broker_feeder_->Run(); }
void SeatDataFeeder::Shutdown() { broker_feeder_->Shutdown(); }

}  // namespace seat_service
}  // namespace sdv
