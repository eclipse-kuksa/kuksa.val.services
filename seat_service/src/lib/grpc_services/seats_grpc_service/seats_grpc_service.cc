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
 * @file      seats_grpc_service.cc
 * @brief     File contains
 */

#include <memory>

#include "seats_grpc_service.h"
#include "seat_adjuster.h"

namespace sdv {
namespace comfort {

// minimise namespace change impact
using ::sdv::edge::comfort::seats::v1::Seat;
using ::sdv::edge::comfort::seats::v1::SeatComponent;
using ::sdv::edge::comfort::seats::v1::SeatLocation;
using ::sdv::edge::comfort::seats::v1::Position;
using ::sdv::edge::comfort::seats::v1::MoveRequest;
using ::sdv::edge::comfort::seats::v1::MoveReply;
using ::sdv::edge::comfort::seats::v1::MoveComponentRequest;
using ::sdv::edge::comfort::seats::v1::MoveComponentReply;
using ::sdv::edge::comfort::seats::v1::CurrentPositionRequest;
using ::sdv::edge::comfort::seats::v1::CurrentPositionReply;

static ::grpc::Status SetResult_2_grpcStatus(SetResult result) {
    switch (result) {
    case SetResult::OK:
        return grpc::Status::OK;
    case SetResult::UNSPECIFIC_ERROR:
        return grpc::Status(grpc::StatusCode::INTERNAL, "Unspecific error");
    case SetResult::NO_CAN:
        return grpc::Status(grpc::StatusCode::INTERNAL, "SocketCAN not available");
    case SetResult::CAN_IF_INDEX_ERROR:
        return grpc::Status(grpc::StatusCode::INTERNAL, "CAN interface index error");
    case SetResult::CAN_BIND_ERROR:
        return grpc::Status(grpc::StatusCode::INTERNAL, "SocketCAN bind() error");
    case SetResult::CAN_IO_ERROR:
        return grpc::Status(grpc::StatusCode::INTERNAL, "SocketCAN i/o error");
    case SetResult::INVALID_ARG:
        return grpc::Status(grpc::StatusCode::INTERNAL, "Invalid argument(s)");
    case SetResult::NO_FRAMES:
        return grpc::Status(grpc::StatusCode::INTERNAL, "Can signals not coming from ECU");
    default:
        return grpc::Status(grpc::StatusCode::INTERNAL, "Unknown internal error");
    }
}

/**
 * @brief
 *
 */
SeatServiceImpl::SeatServiceImpl(std::shared_ptr<SeatAdjuster> adjuster)
    : adjuster_(adjuster) {}

/**
 * @brief Set the desired seat position
 *
 */
::grpc::Status SeatServiceImpl::Move(::grpc::ServerContext* context,
                                     const MoveRequest* request,
                                     MoveReply* response) {
    std::ignore = context;
    std::ignore = response;

    auto location = request->seat().location();
    if (location.row() != 1 || location.index() != 1) {
        return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Unknown seat location");
    }
    auto position = request->seat().position();
    if (position.base() < 0 || 1000 < position.base()) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid base position");
    }

    int position_in_percent = (position.base() + 5) / 10;
    auto result = adjuster_->SetSeatPosition(position_in_percent);
    return SetResult_2_grpcStatus(result);
}

/**
 * @brief Set a seat component position
 *
 */
::grpc::Status SeatServiceImpl::MoveComponent(::grpc::ServerContext* context,
                                              const MoveComponentRequest* request,
                                              MoveComponentReply* response) {
    std::ignore = context;
    std::ignore = response;

    auto location = request->seat();
    if (location.row() != 1 || location.index() != 1) {
        return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Unknown seat location");
    }
    auto component = request->component();
    if (component != SeatComponent::BASE) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Unsupported seat component");
    }
    auto base_position = request->position();
    if (base_position < 0 || 1000 < base_position) {
        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid base position");
    }

    int base_position_in_percent = (base_position + 5) / 10;
    auto result = adjuster_->SetSeatPosition(base_position_in_percent);
    return SetResult_2_grpcStatus(result);
}

/**
 * @ Get the current position of the seat
 *
 */
::grpc::Status SeatServiceImpl::CurrentPosition(::grpc::ServerContext* context,
                                                const CurrentPositionRequest* request,
                                                CurrentPositionReply* response) {
    std::ignore = context;
    if (request->row() != 1 || request->index() != 1) {
        return grpc::Status(grpc::StatusCode::OUT_OF_RANGE, "Unknown seat location");
    }

    auto seat = response->mutable_seat(); // ensure seat is allocated in response
    auto location = seat->mutable_location();
    location->set_index(request->index());
    location->set_row(request->row());

    auto position = seat->mutable_position();
    // Invalidate component positions
    position->set_base(-1);
    position->set_cushion(-1);
    position->set_lumbar(-1);
    position->set_side_bolster(-1);
    position->set_head_restraint(-1);

    auto base_position_in_percent = adjuster_->GetSeatPosition();
    if (base_position_in_percent != SEAT_POSITION_INVALID) {
        position->set_base(base_position_in_percent * 10);
    }

    // Status OK, but Position[component] could be unavailable (-1)
    return grpc::Status::OK;
}

}  // namespace comfort
}  // namespace sdv
