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
 * @file      seat_position_subscriber.cc
 * @brief     (See seat_position_subscriber.h)
 *
 */
#include "seat_position_subscriber.h"

#include <iostream>
#include <memory>
#include <string>

#include "collector_client.h"
#include "seat_adjuster.h"

namespace sdv {
namespace seat_service {

SeatPositionSubscriber::SeatPositionSubscriber(std::shared_ptr<SeatAdjuster> seat_adjuster,
                                               std::shared_ptr<broker_feeder::CollectorClient> collector_client)
    : seat_adjuster_(seat_adjuster)
    , collector_client_(collector_client) {
    /* Define datapoints (metadata) of seat service
     */
    seat_pos_name_ = "Vehicle.Cabin.Seat.Row1.Pos1.Position";
}

void SeatPositionSubscriber::Run() {
    std::cout << "SeatPositionSubscriber::Run()" << std::endl;

    sdv::databroker::v1::SubscribeActuatorTargetRequest request;
    request.add_paths(seat_pos_name_);

    sdv::databroker::v1::SubscribeActuatorTargetReply reply;
    subscriber_context_ = collector_client_->createClientContext();
    std::unique_ptr<::grpc::ClientReader<sdv::databroker::v1::SubscribeActuatorTargetReply>> reader(
        collector_client_->SubscribeActuatorTargets(subscriber_context_.get(), request));
    while (reader->Read(&reply)) {
        std::cout << "got an actuator target" << std::endl;
        for (auto& pair : reply.actuator_targets()) {
            auto path = pair.first;
            auto actuator_target = pair.second;

            switch (actuator_target.value_case()) {
                case sdv::databroker::v1::Datapoint::ValueCase::kInt32Value: {
                    std::cout << actuator_target.int32_value() << std::endl;
                    seat_adjuster_->SetSeatPosition(actuator_target.int32_value());
                } break;
                case sdv::databroker::v1::Datapoint::ValueCase::kUint32Value: {
                    std::cout << actuator_target.uint32_value() << std::endl;
                    seat_adjuster_->SetSeatPosition(actuator_target.int32_value());
                }
            }
        }
    }
    std::cout << "SeatPositionSubscriber::Run exiting" << std::endl;
}

void SeatPositionSubscriber::Shutdown() {
    if (subscriber_context_) {
        subscriber_context_->TryCancel();
    }
}

}  // namespace seat_service
}  // namespace sdv
