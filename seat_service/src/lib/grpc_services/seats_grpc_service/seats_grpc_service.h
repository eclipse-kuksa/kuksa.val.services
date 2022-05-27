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
 * @file      seats_grpc_service.h
 * @brief     File contains 
 *
 */
#pragma once

#include <memory>

#include "sdv/edge/comfort/seats/v1/seats.grpc.pb.h"

namespace sdv {

// fwd decls
class SeatAdjuster;

namespace comfort {

/**
 * @brief 
 * 
 */
class SeatServiceImpl final :
    public sdv::edge::comfort::seats::v1::Seats::Service {
public:
    SeatServiceImpl(std::shared_ptr<SeatAdjuster> adjuster);

    // Set the desired seat position
    ::grpc::Status Move(::grpc::ServerContext* context, const ::sdv::edge::comfort::seats::v1::MoveRequest* request,
                        ::sdv::edge::comfort::seats::v1::MoveReply* response) override;
    // Set a seat component position
    ::grpc::Status MoveComponent(::grpc::ServerContext* context, const ::sdv::edge::comfort::seats::v1::MoveComponentRequest* request,
                                 ::sdv::edge::comfort::seats::v1::MoveComponentReply* response) override;
    // Get the current position of the seat
    ::grpc::Status CurrentPosition(::grpc::ServerContext* context,
                                   const ::sdv::edge::comfort::seats::v1::CurrentPositionRequest* request,
                                   ::sdv::edge::comfort::seats::v1::CurrentPositionReply* response) override;

private:
    std::shared_ptr<SeatAdjuster> adjuster_;
};

}  // namespace comfort
}  // namespace sdv
