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

#include "data_broker_feeder.h"
#include "seat_adjuster.h"

extern int debug;

namespace sdv {
namespace seat_service {

using sdv::databroker::v1::Datapoint;
using sdv::databroker::v1::DataType;
using sdv::databroker::v1::EntryType;
using sdv::databroker::v1::ChangeType;
using sdv::databroker::v1::Datapoint_Failure;

using sdv::broker_feeder::DatapointConfiguration;

/*
const sdv::broker_feeder::DatapointConfiguration metadata_4 {
    { "Vehicle.Cabin.Seat.Row1.DriverSide.Position",
        DataType::UINT16,
        // EntryType::ENTRY_TYPE_ACTUATOR, // entry type can't be set with current API
        ChangeType::ON_CHANGE,
        broker_feeder::createNotAvailableValue(),
        "Seat position on vehicle x-axis. Position is relative to the frontmost position supported by the seat. 0 = Frontmost position supported."
    },
    { "Vehicle.Cabin.SeatRowCount",
        DataType::UINT8,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        broker_feeder::createDatapoint(2U),
        "Number of seat rows in vehicle."},
    { "Vehicle.Cabin.SeatPosCount",
        DataType::UINT8_ARRAY,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        broker_feeder::createDatapoint(std::vector<uint32_t> {2U, 3U}),
        "Number of seats across each row from the front to the rear."
    },
};

const sdv::broker_feeder::DatapointConfiguration metadata_3 {
    { "Vehicle.Cabin.Seat.Row1.Pos1.Position",
        DataType::UINT16, // Changed from UINT32 to match VSS 3.0
        ChangeType::ON_CHANGE,
        broker_feeder::createNotAvailableValue(),
        "Longitudinal position of overall seat"
    },
    { "Vehicle.Cabin.SeatRowCount",
        DataType::UINT8,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        broker_feeder::createDatapoint(2U),
        "Number of rows of seats"
    },
    { "Vehicle.Cabin.SeatPosCount",
        DataType::UINT8_ARRAY,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        broker_feeder::createDatapoint(std::vector<uint32_t> {2U, 3U}),
        "Number of seats across each row from the front to the rear."
    }
};
*/

SeatDataFeeder::SeatDataFeeder(std::shared_ptr<SeatAdjuster> seat_adjuster, std::shared_ptr<broker_feeder::KuksaClient> collector_client,
                               std::string& seat_pos_name, std::string& seat_tilt_name, std::string& seat_height_name, DatapointConfiguration&& dpConfig)
    : seat_adjuster_(seat_adjuster)
{
    /* Init feeder
     */
    broker_feeder_ = sdv::broker_feeder::DataBrokerFeeder::createInstance(collector_client, std::move(dpConfig));

    /* Internally subscribe to signals to be fed to broker
     */
    // handle forward motor position
    seat_adjuster_->SubscribePosition([this, seat_pos_name](int position_in_percent) {
        const std::string self = "[SeatSvc][SeatDataFeeder] ";
        if (debug > 1) { // require more verbose for extra dump
            std::cout << self << "got pos: " << position_in_percent << "%" << std::endl;
        }
        Datapoint datapoint;
        // NOTE: we are using uint32 value as grpc does not have smaller integers
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

    // handle tilt motor position
    seat_adjuster_->SubscribeTilt([this, seat_tilt_name](int position_in_percent) {
        const std::string self = "[SeatSvc][SeatDataFeeder] ";
        if (debug > 1) { // require more verbose for extra dump
            std::cout << self << "got pos: " << position_in_percent << "%" << std::endl;
        }
        Datapoint datapoint;
        if (0 <= position_in_percent && position_in_percent <= 100) {
            datapoint.set_float_value(static_cast<float>(position_in_percent * 10)); // scale up to [0..1000]
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
                    << "FeedValue(" << seat_tilt_name << ", failure:"
                    << Datapoint_Failure_Name(datapoint.failure_value()) << ")" << std::endl;
            } else
            if (datapoint.has_float_value()) {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_tilt_name << ", float:" << datapoint.float_value() << ")" << std::endl;
            } else {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_tilt_name << ", unknown)" << std::endl;
            }
        }
        broker_feeder_->FeedValue(seat_tilt_name, datapoint);
    });

    // handle height motor position
    seat_adjuster_->SubscribeHeight([this, seat_height_name](int position_in_percent) {
        const std::string self = "[SeatSvc][SeatDataFeeder] ";
        if (debug > 1) { // require more verbose for extra dump
            std::cout << self << "got pos: " << position_in_percent << "%" << std::endl;
        }
        Datapoint datapoint;
        // NOTE: we are using uint32 value as grpc does not have smaller integers
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
                    << "FeedValue(" << seat_height_name << ", failure:"
                    << Datapoint_Failure_Name(datapoint.failure_value()) << ")" << std::endl;
            } else
            if (datapoint.has_uint32_value()) {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_height_name << ", uint32:" << datapoint.uint32_value() << ")" << std::endl;
            } else {
                std::cout << self << "pos: " << position_in_percent << "% -> "
                    << "FeedValue(" << seat_height_name << ", unknown)" << std::endl;
            }
        }
        broker_feeder_->FeedValue(seat_height_name, datapoint);
    });
}
void SeatDataFeeder::Run() { broker_feeder_->Run(); }
void SeatDataFeeder::Shutdown() { broker_feeder_->Shutdown(); }

}  // namespace seat_service
}  // namespace sdv
