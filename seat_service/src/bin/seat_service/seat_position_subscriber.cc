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
#include <sstream>
#include <thread>
#include <mutex>

#include "kuksa_client.h"
#include "seat_adjuster.h"

extern int debug;

namespace sdv {
namespace seat_service {

std::mutex seat_adjuster_mutex;

SeatPositionSubscriber::SeatPositionSubscriber(std::shared_ptr<SeatAdjuster> seat_adjuster,
                                               std::shared_ptr<broker_feeder::KuksaClient> kuksa_client,
                                               const std::string& seat_pos_name, const sdv::seat_service::posSub& pos)
    : seat_adjuster_(seat_adjuster)
    , kuksa_client_(kuksa_client)
    , seat_pos_name_(seat_pos_name)
    , pos_(pos)
    , running_(false)
{
    /* Define datapoints (metadata) of seat service */
    std::cout << "SeatPositionSubscriber(" << seat_pos_name_ << ") initialized" << std::endl;
}

void SeatPositionSubscriber::Run() {
    std::cout << "SeatPositionSubscriber::Run()" << std::endl;

    running_ = true;
    int failures = 0; // subscribe errors, if too many subscriber is disabled!
    while (running_) {
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(3);
        if (!kuksa_client_->WaitForConnected(deadline)) {
            if (debug > 1) {
                std::cout << "SeatPositionSubscriber: not connected." << std::endl;
            }
            continue;
        }

        std::cout << "SeatPositionSubscriber: connected." << std::endl;

        kuksa::val::v1::SubscribeRequest request;
        {
            auto entry = request.add_entries();
            entry->set_path(seat_pos_name_);
            entry->add_fields(kuksa::val::v1::Field::FIELD_ACTUATOR_TARGET);
        }

        kuksa::val::v1::SubscribeResponse response;
        subscriber_context_ = kuksa_client_->createClientContext();
        if (debug > 1) {
            std::cout << "SeatPositionSubscriber: Subscribe(" << seat_pos_name_ << ")" << std::endl;
        }
        std::unique_ptr<::grpc::ClientReader<kuksa::val::v1::SubscribeResponse>> reader(
            kuksa_client_->Subscribe(subscriber_context_.get(), request));
        if (debug > 4) {
            std::ostringstream os;
            os << "[GRPC]  VAL.Subscribe(" << request.ShortDebugString() << ")";
            std::cout << os.str() << std::endl;
        }
        while (reader->Read(&response)) {
            if (debug > 4) {
                std::ostringstream os;
                os << "[GRPC]  VAL.ClientReader() -> \n  " << response.ShortDebugString();
                std::cout << os.str() << std::endl;
            }
            for (auto& update : response.updates()) {
                if (update.entry().path() == seat_pos_name_) {
                    auto actuator_target = update.entry().actuator_target();
                    switch (actuator_target.value_case()) {
                        case sdv::databroker::v1::Datapoint::ValueCase::kUint32Value: {
                            auto position = actuator_target.uint32();
                            std::cout << "SeatPositionSubscriber: Got actuator target: " << position << std::endl;
                            if (position < 0 || 1000 < position) {
                                std::cout << "Invalid position" << std::endl;
                                continue;
                            }

                            int position_in_percent = (position + 5) / 10;

                            std::lock_guard<std::mutex> guard(seat_adjuster_mutex);
                            switch(pos_){
                                case posSub::POSITION:
                                    seat_adjuster_->SetSeatPosition(position_in_percent);
                                    break;
                                case posSub::TILT:
                                    seat_adjuster_->SetSeatTilt(position_in_percent);
                                    break;
                                case posSub::HEIGHT:
                                    seat_adjuster_->SetSeatHeight(position_in_percent);
                                    break;
                            }
                            break;
                        }
                        case sdv::databroker::v1::Datapoint::ValueCase::kFloatValue: {
                            auto position = static_cast<unsigned int>(actuator_target.float_());
                            std::cout << "SeatPositionSubscriber: Got actuator target: " << position << std::endl;
                            if (position < 0 || 1000 < position) {
                                std::cout << "Invalid position" << std::endl;
                                continue;
                            }

                            int position_in_percent = (position + 5) / 10;

                            std::lock_guard<std::mutex> guard(seat_adjuster_mutex);
                            switch(pos_){
                                case posSub::POSITION:
                                    seat_adjuster_->SetSeatPosition(position_in_percent);
                                    break;
                                case posSub::TILT:
                                    seat_adjuster_->SetSeatTilt(position_in_percent);
                                    break;
                                case posSub::HEIGHT:
                                    seat_adjuster_->SetSeatHeight(position_in_percent);
                                    break;
                            }
                            break;
                        }
                    }
                }
            }
        }
        if (debug > 3) {
            std::cout << "SeatPositionSubscriber: Reader->Read() -> false" << std::endl;
        }
        grpc::Status status = reader->Finish();
        if (status.ok()) {
            std::cout << "SeatPositionSubscriber: disconnected." << std::endl;
            failures = 0; // reset subscribe failures affter successful finish
        } else {
            std::ostringstream os;
            os << "SeatPositionSubscriber(" << seat_pos_name_ << "): Disconnected with "
               << sdv::utils::toString(status);
            std::cerr << os.str() << std::endl;

            if (status.error_code() == grpc::StatusCode::NOT_FOUND) {
                failures++;
                std::cerr << "SeatPositionSubscriber: Path not found: " << seat_pos_name_ << ". Attempt: " << failures << std::endl;
                if (failures > 3) {
                    std::cerr << "\nWARNING!" << std::endl;
                    std::cerr << "SeatPositionSubscriber() Aborted. Actuator " << seat_pos_name_ << " is permanently unavailable!\n\n" << std::endl;
                    running_ = false;
                    break;
                }
            }
            // prevent busy polling if subscribe failed with error
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        if (debug > 2) {
            std::cout << "SeatPositionSubscriber: subscriber_context_ = null" << std::endl;
        }
        subscriber_context_ = nullptr;
    }
    if (debug > 0) {
        std::cout << "SeatPositionSubscriber: exiting" << std::endl;
    }
}

void SeatPositionSubscriber::Shutdown() {
    running_ = false;
    if (subscriber_context_) {
        subscriber_context_->TryCancel();
    }
}

}  // namespace seat_service
}  // namespace sdv
