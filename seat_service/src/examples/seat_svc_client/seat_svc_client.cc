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
 * @file      seat_svc_client.cc
 * @brief     File contains
 */

#include <grpcpp/grpcpp.h>

#include <getopt.h>

#include <cstdint>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "sdv/edge/comfort/seats/v1/seats.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::StatusCode;
using sdv::edge::comfort::seats::v1::MoveComponentReply;
using sdv::edge::comfort::seats::v1::MoveComponentRequest;
using sdv::edge::comfort::seats::v1::CurrentPositionReply;
using sdv::edge::comfort::seats::v1::CurrentPositionRequest;
using sdv::edge::comfort::seats::v1::SeatLocation;
using sdv::edge::comfort::seats::v1::SeatComponent;
using sdv::edge::comfort::seats::v1::Seats;
using sdv::edge::comfort::seats::v1::Position;

/**
 * @brief grpc_status_code_to_string
 */
const char* grpc_status_code_to_string(StatusCode status);

static int debug = ::getenv("CLI_DEBUG") && atoi(::getenv("CLI_DEBUG"));;

static void print_grpc_status(const Status &status) {
    if (status.ok()) {
        std::cout << "GRPC: OK" << std::endl;
    } else {
        std::cerr << "GRPC error: { code:" << status.error_code() << " ("
                << grpc_status_code_to_string(status.error_code())
                << ") \"" << status.error_message() << "\" }" << std::endl;
    }
}

static void print_postition(const Position &position) {
    std::cout << "Postition: { " <<
        "Base:" << position.base() << ", " <<
        "Cushion:" << position.cushion() << ", " <<
        "Lumbar:" << position.lumbar() << ", " <<
        "SideBolster:" << position.side_bolster() << ", " <<
        "HeadRestraint:" << position.head_restraint() << " }" << std::endl;
}

static int get_component_position(const SeatComponent seat_comp, const Position &position) {
    switch (seat_comp) {
        case SeatComponent::BASE:
            return position.base();
        case SeatComponent::CUSHION:
            return position.cushion();
        case SeatComponent::LUMBAR:
            return position.lumbar();
        case SeatComponent::SIDE_BOLSTER:
            return position.side_bolster();
        case SeatComponent::HEAD_RESTRAINT:
            return position.head_restraint();
        default:
            return -1;
    }
}

/**
 * @brief SeatSvcClient
 */
class SeatSvcClient {
   public:
    SeatSvcClient(std::shared_ptr<Channel> channel)
        : _stub(Seats::NewStub(channel)) {}

    virtual ~SeatSvcClient() {}

    Status CurrentSeatPosition(uint32_t row, uint32_t index, Position &position) {
        CurrentPositionRequest request;
        request.set_row(row);
        request.set_index(index);

        // Container for the data we expect from the server.
        CurrentPositionReply reply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // Invalidate returned position
        position.set_base(-1);
        position.set_cushion(-1);
        position.set_lumbar(-1);
        position.set_side_bolster(-1);
        position.set_head_restraint(-1);

        // The actual RPC.
        Status status = _stub->CurrentPosition(&context, request, &reply);

        // Act upon its status.
        if (!status.ok()) {
            std::cerr << "[CurrentSeatPosition] failed (" << status.error_code() << ")" << std::endl;
            //print_grpc_status(status);
        } else {
            if (reply.has_seat() && reply.seat().has_position()) {
                position = reply.seat().position(); // using operator=
                if (debug) print_postition(position);
            }
        }
        return status;
    }

    Status MoveSeatPosition(SeatComponent component, uint32_t row, uint32_t index, int32_t position) {
        MoveComponentRequest request;
        request.mutable_seat()->set_row(row);
        request.mutable_seat()->set_index(index);
        request.set_component(component);
        request.set_position(position);

        // Container for the data we expect from the server.
        MoveComponentReply reply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The actual RPC.
        Status status = _stub->MoveComponent(&context, request, &reply);

        // Act upon its status.
        if (!status.ok()) {
            std::cerr << "[MoveSeatPosition] failed (" << status.error_code() << ")" << std::endl;
            //print_grpc_status(status);
        }
        return status;
    }

    //    private:
    std::unique_ptr<Seats::Stub> _stub;
};

/**
 * @brief grpc_status_code_to_string
 */
const char* grpc_status_code_to_string(StatusCode status) {
    switch (status) {
        case StatusCode::OK:
            return "OK";
        case StatusCode::CANCELLED:
            return "CANCELLED";
        case StatusCode::UNKNOWN:
            return "UNKNOWN";
        case StatusCode::INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case StatusCode::DEADLINE_EXCEEDED:
            return "DEADLINE_EXCEEDED";
        case StatusCode::NOT_FOUND:
            return "NOT_FOUND";
        case StatusCode::ALREADY_EXISTS:
            return "ALREADY_EXISTS";
        case StatusCode::PERMISSION_DENIED:
            return "PERMISSION_DENIED";
        case StatusCode::RESOURCE_EXHAUSTED:
            return "RESOURCE_EXHAUSTED";
        case StatusCode::FAILED_PRECONDITION:
            return "FAILED_PRECONDITION";
        case StatusCode::ABORTED:
            return "ABORTED";
        case StatusCode::OUT_OF_RANGE:
            return "OUT_OF_RANGE";
        case StatusCode::UNIMPLEMENTED:
            return "UNIMPLEMENTED";
        case StatusCode::INTERNAL:
            return "INTERNAL";
        case StatusCode::UNAVAILABLE:
            return "UNAVAILABLE";
        case StatusCode::DATA_LOSS:
            return "DATA_LOSS";
        case StatusCode::UNAUTHENTICATED:
            return "UNAUTHENTICATED";
        default:
            return "UNKNOWN";
    }
}

void print_usage() {
    std::cout << "Usage: ./seat_svc_client OPTIONS POSITION" << std::endl << std::endl;

    std::cout << "OPTIONS:"  << std::endl;
    std::cout << "   -t, --target : GRPC address for SeatService <ip:port>. Default: localhost:50051" << std::endl;
    std::cout << "   -c, --comp   : SeatComponent value [0=BASE, 1=CUSHION, etc..]. Default: 0" << std::endl;
    std::cout << "   -r, --row    : Seat Location Row (1=Front). Default: 1 " << std::endl;
    std::cout << "   -i, --ind    : Seat Location Index in a Row (1=Left). Default: 1 " << std::endl;
    std::cout << "   -g, --get    : Get Current Seat Position" << std::endl;
    std::cout << "   -w, --wait   : Wait to reach target position (via polling)" << std::endl;
    std::cout << "   -h, --help   : Prints this message" << std::endl << std::endl;

    std::cout << "ARGUMENTS:"  << std::endl;
    std::cout << "   POSITION     : Seat Position in range [0..1000]. Default: 500" << std::endl;
}

/**
 * @brief main Instantiate the client. It requires a channel, out of which the actual RPCs
 * are created. This channel models a connection to an endpoint specified by
 * the argument "--target=". If another argument is provided - it is considered
 * as value to set the seat position.
 * We indicate that the channel isn't authenticated (use of InsecureChannelCredentials()).
 */
int main(int argc, char** argv) {

    // Default values
    int pos = 500;
    int seat_row = 1;
    int seat_col = 1;
    int wait_position = 0;
    int get_pos = 0;
    int seat_comp = SeatComponent::BASE;
    std::string target_str = "localhost:50051";

    static struct option long_options[] = {
        // "<name>", int <has_arg>, int* <flag>,  <val=short_opt>
        {"target",  required_argument, NULL, 't' },
        {"comp",    required_argument, NULL, 'c' },
        {"row",     required_argument, NULL, 'r' },
        {"ind",     required_argument, NULL, 'i' },
        {"get",     no_argument,       NULL, 'g' },
        {"wait",    no_argument,       NULL, 'w' },
        {"help",    no_argument,       NULL, 'h' },
        {0, 0, 0, 0 }
    };

    while (true) {
        int opt = getopt_long(argc, argv, "t:c:r:i:gwh", long_options, NULL);
        if (opt == -1) break;
        switch (opt) {
            case 't':
                target_str = optarg;
                break;
            case 'r':
                seat_row = std::atoi(optarg);
                break;
            case 'i':
                seat_col = std::atoi(optarg);
                break;
            case 'c':
                seat_comp = std::atoi(optarg);
                if (!sdv::edge::comfort::seats::v1::SeatComponent_IsValid(seat_comp)) {
                    std::cerr << "Invalid SeatComponent value: " << seat_comp << std::endl;
                    print_usage();
                    exit(1);
                }
                break;
            case 'h':
                print_usage();
                exit(0);
                break;
            case 'g':
                get_pos = 1;
                break;
            case 'w':
                wait_position = 1;
                break;
            case '?': // unknown argument, warning printed already
                print_usage();
                exit(1);
                break;
        }
    }
    // optind - 1st non-option argument
    if (optind < argc) {
        // consider argument as value
        pos = std::atoi(argv[optind]);
    }

    auto seatClient = std::make_unique<SeatSvcClient>(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

    // FIXME: make seat lcoation configurable
    SeatComponent sc = SeatComponent(seat_comp);

    // first get current position
    if (get_pos) {
        std::cout << std::endl;
        Position position;
        std::cout << "*** Seats.CurrentSeatPosition(" << seat_row << ", " << seat_col << ")" << std::endl;
        auto get_result = seatClient->CurrentSeatPosition(seat_row, seat_col, position);
        std::cout << "  --> "; print_grpc_status(get_result);
        if (get_result.ok()) {
            std::cout << "  --> "; print_postition(position); 
        }
        std::cout << std::endl;
    }

    std::cout << "*** Seats.MoveComponent(SeatComponent::" << SeatComponent_Name(sc) << ", " <<
                 seat_row << ", " << seat_col << ", " << pos << ")" << std::endl;

    auto move_result = seatClient->MoveSeatPosition((SeatComponent)seat_comp, seat_row, seat_col, pos);
    print_grpc_status(move_result);
    //std::cout << "*** StatusCode: " << grpc_status_code_to_string(result.error_code()) << std::endl;

    int retries = 0;
    if (move_result.ok() && wait_position) {
        Position position;
        int current_pos = -1;

        std::cout << std::endl << "*** Waiting for seat target position..." << std::endl;
        while (true) {
            auto result = seatClient->CurrentSeatPosition(seat_row, seat_col, position);
            if (!result.ok()) {
                print_grpc_status(result);
                std::cout << std::endl << "*** Waiting aborted." << std::endl;
                break;
            }
            current_pos = get_component_position(sc, position);
            print_postition(position);
            if (pos / 10 == current_pos / 10) {
                std::cout << "***  SeatComponent::" << SeatComponent_Name(sc) << " reached: " << current_pos << std::endl << std::endl;
                return 0;
            }
            retries++;
            if (retries * 100 > 15000L) { // wait up to 15s.
                std::cout << std::endl << "*** Waiting aborted (Timeout)." << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    seatClient.release(); // to ensure grpc objects are destroyed before grpc_shutdown

    // std::cout << "- grpc_shutdown() ..." << std::endl;
    // grpc_shutdown_blocking();

    if (debug) {
        std::cout << "- ShutdownProtobufLibrary() ..." << std::endl;
    }
    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
