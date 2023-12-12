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


#include <fcntl.h>  // fcntl
#include <grpcpp/grpcpp.h>
#include <unistd.h>  // pipe

#include <csignal>  // std::signal
#include <thread>

#include "seat_adjuster.h"
#include "seat_data_feeder.h"
#include "seat_position_subscriber.h"
#include "seats_grpc_service.h"
#include "data_broker_feeder.h"
#include "create_datapoint.h"
#include "kuksa_client.h"

#define SELF "[SeatSvc] "

int debug = std::stoi(sdv::utils::getEnvVar("SEAT_DEBUG", "1"));

using sdv::databroker::v1::Datapoint;
using sdv::databroker::v1::DataType;
using sdv::databroker::v1::EntryType;
using sdv::databroker::v1::ChangeType;

/**
 * NOTE: VSS 4.0 and 3.0 differ only on driver's seat position, but all dataponits are registered
 * for compatiblity purposes and future-proofing (e.g. some data type, description changes)
 *
 * Although datapoints are registered, there is no possibility to set EntryType (actuator) in current API, so
 * it is suggested to use seat service with vss3/4 configured databroker.
 *
 * In case databroker runs without proper actuator datapoint, it won't be possible to subscribe on actuator changes.
 */

const std::string SEAT_POS_VSS_3 = "Vehicle.Cabin.Seat.Row1.Pos1.Position";
const std::string SEAT_POS_VSS_4 = "Vehicle.Cabin.Seat.Row1.DriverSide.Position";
const std::string SEAT_TILT_VSS_3 = "Vehicle.Cabin.Seat.Row1.Pos1.Tilt";
const std::string SEAT_TILT_VSS_4 = "Vehicle.Cabin.Seat.Row1.DriverSide.Tilt";
const std::string SEAT_HEIGHT_VSS_3 = "Vehicle.Cabin.Seat.Row1.Pos1.Height";
const std::string SEAT_HEIGHT_VSS_4 = "Vehicle.Cabin.Seat.Row1.DriverSide.Height";

const sdv::broker_feeder::DatapointConfiguration metadata_4 {
    { SEAT_POS_VSS_4,
        DataType::UINT16,
        // EntryType::ENTRY_TYPE_ACTUATOR, // entry type can't be set with current API
        ChangeType::ON_CHANGE,
        sdv::broker_feeder::createNotAvailableValue(),
        "Seat position on vehicle x-axis. Position is relative to the frontmost position supported by the seat. 0 = Frontmost position supported."
    },
    { SEAT_TILT_VSS_4,
        DataType::UINT16,
        // EntryType::ENTRY_TYPE_ACTUATOR, // entry type can't be set with current API
        ChangeType::ON_CHANGE,
        sdv::broker_feeder::createNotAvailableValue(),
        "Tilting of seat (seating and backrest) relative to vehicle x-axis. 0 = seat bottom is flat, seat bottom and vehicle x-axis are parallel. Positive degrees = seat tilted backwards, seat x-axis tilted upward, seat z-axis is tilted backward."
    },
    { SEAT_HEIGHT_VSS_4,
        DataType::UINT16,
        // EntryType::ENTRY_TYPE_ACTUATOR, // entry type can't be set with current API
        ChangeType::ON_CHANGE,
        sdv::broker_feeder::createNotAvailableValue(),
        "Seat position on vehicle z-axis. Position is relative within available movable range of the seating. 0 = Lowermost position supported."
    },
    { "Vehicle.Cabin.SeatRowCount",
        DataType::UINT8,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        sdv::broker_feeder::createDatapoint(2U),
        "Number of seat rows in vehicle."},
    { "Vehicle.Cabin.SeatPosCount",
        DataType::UINT8_ARRAY,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        sdv::broker_feeder::createDatapoint(std::vector<uint32_t> {2U, 3U}),
        "Number of seats across each row from the front to the rear."
    },
};

const sdv::broker_feeder::DatapointConfiguration metadata_3 {
    { SEAT_POS_VSS_3,
        DataType::UINT16, // Changed from UINT32 to match VSS 3.0
        ChangeType::ON_CHANGE,
        sdv::broker_feeder::createNotAvailableValue(),
        "Longitudinal position of overall seat"
    },
    { SEAT_TILT_VSS_3,
        DataType::UINT16,
        // EntryType::ENTRY_TYPE_ACTUATOR, // entry type can't be set with current API
        ChangeType::ON_CHANGE,
        sdv::broker_feeder::createNotAvailableValue(),
        "Tilting of seat (seating and backrest) relative to vehicle x-axis. 0 = seat bottom is flat, seat bottom and vehicle x-axis are parallel. Positive degrees = seat tilted backwards, seat x-axis tilted upward, seat z-axis is tilted backward."
    },
    { SEAT_HEIGHT_VSS_3,
        DataType::UINT16,
        // EntryType::ENTRY_TYPE_ACTUATOR, // entry type can't be set with current API
        ChangeType::ON_CHANGE,
        sdv::broker_feeder::createNotAvailableValue(),
        "Seat position on vehicle z-axis. Position is relative within available movable range of the seating. 0 = Lowermost position supported."
    },
    { "Vehicle.Cabin.SeatRowCount",
        DataType::UINT8,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        sdv::broker_feeder::createDatapoint(2U),
        "Number of rows of seats"
    },
    { "Vehicle.Cabin.SeatPosCount",
        DataType::UINT8_ARRAY,
        // EntryType::ENTRY_TYPE_ATTRIBUTE,
        ChangeType::STATIC,
        sdv::broker_feeder::createDatapoint(std::vector<uint32_t> {2U, 3U}),
        "Number of seats across each row from the front to the rear."
    }
};


// Self pipe (for signal handling)
int pipefd[2];

void signal_handler(int signal) {
    char sig = signal;
    while (write(pipefd[1], &sig, sizeof(sig)) < 0) {
        if (errno == EINTR) {
            // If write was interupted by a signal, try again.
        } else {
            // Otherwise it doesn't make much sense to try again.
            break;
        }
    }
}

int setup_signal_handler() {
    // Setup signal handler (using a self pipe)
    if (pipe(pipefd) == -1) {
        std::cout << SELF "Failed to setup signal handler (self pipe)" << std::endl;
        std::exit(1);
    }

    auto pipe_read_fd = pipefd[0];
    auto pipe_write_fd = pipefd[1];

    // Set write end of pipe to non-blocking in order for it to work reliably in
    // the signal handler. We _do_ want the read end to remain blocking so we
    // can block while waiting for a signal.
    int flags = fcntl(pipe_write_fd, F_GETFL) | O_NONBLOCK;
    if (fcntl(pipe_write_fd, F_SETFL, flags) != 0) {
        std::cout << SELF "Failed to set self pipe to non blocking" << std::endl;
        std::exit(1);
    }
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    return pipe_read_fd;  // return read end of pipe
}

// Blocks until signal is received
void wait_for_signal(int fd) {
    char buf;
    auto res = read(fd, &buf, sizeof(buf));
    if (res < 0) {
        perror(SELF"[wait_for_signal] read() error");
    } else if (res == 1) {
        std::cout << SELF "[wait_for_signal] Received signal: " << (int) buf << std::endl;
    } else {
        std::cout << SELF "[wait_for_signal] unexpected EOF" << std::endl;
    }
}


void Run(std::string can_if_name, std::string listen_address, std::string port, std::string broker_addr, bool vss_4) {

    sdv::broker_feeder::DatapointConfiguration metadata = vss_4 ? metadata_4 : metadata_3;

    std::string seat_pos_name = vss_4 ? SEAT_POS_VSS_4 : SEAT_POS_VSS_3;
    std::string seat_tilt_name = vss_4 ? SEAT_TILT_VSS_4 : SEAT_TILT_VSS_3;
    std::string seat_height_name = vss_4 ? SEAT_HEIGHT_VSS_4 : SEAT_HEIGHT_VSS_3;

    // runtime check for valid 1st entry name
    if (metadata.size() < 1 || seat_pos_name != metadata[0].name || seat_tilt_name != metadata[1].name || seat_height_name != metadata[2].name) {
        std::cerr << SELF "Invalid metadata configuration!" << std::endl;
        exit(1);
    }

    auto seat_adjuster = sdv::SeatAdjuster::createInstance(can_if_name);
    auto client = sdv::broker_feeder::KuksaClient::createInstance(broker_addr);

    // Setup feeder
    //
    sdv::seat_service::SeatDataFeeder seat_data_feeder(seat_adjuster, client, seat_pos_name, seat_tilt_name, seat_height_name, std::move(metadata));
    std::cout << SELF "SeatDataFeeder connecting to " << broker_addr << std::endl;
    std::thread feeder_thread(&sdv::seat_service::SeatDataFeeder::Run, &seat_data_feeder);


    // Setup target actuator subscriber
    sdv::seat_service::SeatPositionSubscriber seat_position_subscriber(seat_adjuster, client, seat_pos_name, sdv::seat_service::posSub::POSITION);
    std::cout << SELF "Start seat position subscription " << broker_addr << std::endl;

    // Setup target actuator subscriber
    sdv::seat_service::SeatPositionSubscriber seat_tilt_subscriber(seat_adjuster, client, seat_tilt_name, sdv::seat_service::posSub::TILT);
    std::cout << SELF "Start seat tilt subscription " << broker_addr << std::endl;

    // Setup target actuator subscriber
    sdv::seat_service::SeatPositionSubscriber seat_height_subscriber(seat_adjuster, client, seat_height_name, sdv::seat_service::posSub::HEIGHT);
    std::cout << SELF "Start seat height subscription " << broker_addr << std::endl;

    std::thread subscriber1_thread(&sdv::seat_service::SeatPositionSubscriber::Run, &seat_position_subscriber);
    std::thread subscriber2_thread(&sdv::seat_service::SeatPositionSubscriber::Run, &seat_tilt_subscriber);
    std::thread subscriber3_thread(&sdv::seat_service::SeatPositionSubscriber::Run, &seat_height_subscriber);

    // Setup grpc server and register the services
    //
    sdv::comfort::SeatServiceImpl seat_service(seat_adjuster);
    grpc::ServerBuilder builder;
    std::string server_address(listen_address + ":" + port);
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&seat_service);

    std::shared_ptr<grpc::Server> server(builder.BuildAndStart());
    // fix SIGSEGV if server bind failed
    std::shared_ptr<std::thread> server_thread(nullptr);
    if (server) {
        std::cout << SELF "Server listening on " << server_address << std::endl;
        server_thread = std::shared_ptr<std::thread>(new std::thread(&grpc::Server::Wait, server));

        // Setup signal handler & wait for signal
        auto fd = setup_signal_handler();
        wait_for_signal(fd);

    } else {
        std::cerr << SELF "Server failed to listen on " << server_address << std::endl;
    }

    std::cout << SELF "Shutting down..." << std::endl;

    seat_data_feeder.Shutdown();
    seat_position_subscriber.Shutdown();
    seat_tilt_subscriber.Shutdown();
    seat_height_subscriber.Shutdown();
    if (server) {
        server->Shutdown();
        server_thread->join();
    }
    subscriber1_thread.join();
    subscriber2_thread.join();
    subscriber3_thread.join();
    feeder_thread.join();

    // Optional: Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
}

int main(int argc, char** argv) {
    std::string can_if_name;
    std::string listen_address = "localhost";  // Default listen address
    std::string port = "50051";                // Default listen port
    bool vss_4 = true; // vss 3.0 or 4.0 seat service paths

    switch (argc) {
        case 4:
            port = argv[3];
        case 3:
            listen_address = argv[2];
        case 2:
            can_if_name = argv[1];
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " CAN_IF_NAME [LISTEN_ADDRESS [PORT]]" << std::endl;
            std::cerr << std::endl;
            std::cerr<< "Environment: SEAT_DEBUG=1 to enable SeatDataFeeder dumps" << std::endl;
            return 1;
    }

    // Allow easy overriding of DataFeeder host:port via env (in containers)
    auto broker_addr = sdv::utils::getEnvVar("BROKER_ADDR", "localhost:55555");  // Replace hardcoded broker address and port
    auto vss_val = sdv::utils::getEnvVar("VSS", "4");
    if (vss_val == "3") {
        vss_4 = false;
    } else if (vss_val == "4") {
        vss_4 = true;
    } else {
        std::cerr << "Invalid 'VSS' env: " << vss_val << ". Use: [3, 4]" << std::endl;
        exit(1);
    }

    if (vss_4) {
        std::cout << "### Using VSS 4.0 mode" << std::endl;
    }
    if (debug > 1) {
        std::cout << "### Using GRPC version:" << ::grpc::Version() << std::endl;
    }

    Run(can_if_name, listen_address, port, broker_addr, vss_4);

    return 0;
}
