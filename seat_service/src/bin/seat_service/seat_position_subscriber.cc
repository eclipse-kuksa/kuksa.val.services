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
    , collector_client_(collector_client)
    , running_(false) {
    /* Define datapoints (metadata) of seat service
     */
    seat_pos_name_ = "Vehicle.Cabin.Seat.Row1.Pos1.Position";
}

void SeatPositionSubscriber::Run() {
    std::cout << "SeatPositionSubscriber::Run()" << std::endl;

    running_ = true;
    while (running_) {
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(2);
        if (!collector_client_->WaitForConnected(deadline)) {
            std::cout << "SeatPositionSubscriber: not connected" << std::endl;
            continue;
        }

        std::cout << "SeatPositionSubscriber: connected" << std::endl;

        sdv::databroker::v1::SubscribeActuatorTargetRequest request;
        request.add_paths(seat_pos_name_);

        sdv::databroker::v1::SubscribeActuatorTargetReply reply;
        subscriber_context_ = collector_client_->createClientContext();
        std::unique_ptr<::grpc::ClientReader<sdv::databroker::v1::SubscribeActuatorTargetReply>> reader(
            collector_client_->SubscribeActuatorTargets(subscriber_context_.get(), request));
        while (reader->Read(&reply)) {
            for (auto& pair : reply.actuator_targets()) {
                auto path = pair.first;
                auto actuator_target = pair.second;

                switch (actuator_target.value_case()) {
                    case sdv::databroker::v1::Datapoint::ValueCase::kUint32Value: {
                        auto position = actuator_target.uint32_value();
                        std::cout << "Got actuator target: " << position << std::endl;
                        if (position < 0 || 1000 < position) {
                            std::cout << "Invalid position" << std::endl;
                            continue;
                        }

                        int position_in_percent = (position + 5) / 10;

                        seat_adjuster_->SetSeatPosition(position_in_percent);
                    }
                }
            }
        }
        subscriber_context_ = nullptr;
        std::cout << "SeatPositionSubscriber: disconnected" << std::endl;
    }
    std::cout << "SeatPositionSubscriber: exiting" << std::endl;
}

void SeatPositionSubscriber::Shutdown() {
    running_ = false;
    if (subscriber_context_) {
        subscriber_context_->TryCancel();
    }
}

}  // namespace seat_service
}  // namespace sdv
