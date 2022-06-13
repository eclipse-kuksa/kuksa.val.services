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
#include "seats_grpc_service.h"

#define SELF "[SeatSvc] "

static std::string getEnvVar(const std::string& name, const std::string& defaultValue = {}) {
    const char* value = std::getenv(name.c_str());
    return value != nullptr ? std::string(value) : std::string(defaultValue);
}

int debug = std::stoi(getEnvVar("SEAT_DEBUG", "1"));

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

void Run(std::string can_if_name, std::string listen_address, std::string port, std::string broker_addr) {
    auto seat_adjuster = sdv::SeatAdjuster::createInstance(can_if_name);

    // Setup feeder
    //
    sdv::seat_service::SeatDataFeeder seat_data_feeder(seat_adjuster, broker_addr);
    std::cout << SELF "SeatDataFeeder connecting to " << broker_addr << std::endl;
    std::thread feeder_thread(&sdv::seat_service::SeatDataFeeder::Run, &seat_data_feeder);

    // Setup grpc server and register the services
    //
    sdv::comfort::SeatServiceImpl seat_service(seat_adjuster);
    grpc::ServerBuilder builder;
    std::string server_address(listen_address + ":" + port);
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&seat_service);

    std::shared_ptr<grpc::Server> server(builder.BuildAndStart());

    std::cout << SELF "Server listening on " << server_address << std::endl;
    std::thread server_thread(&grpc::Server::Wait, server);

    // Setup signal handler & wait for signal
    auto fd = setup_signal_handler();
    wait_for_signal(fd);

    std::cout << SELF "Shutting down..." << std::endl;

    seat_data_feeder.Shutdown();
    server->Shutdown();
    server_thread.join();
    feeder_thread.join();

    // Optional: Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();
}

int main(int argc, char** argv) {
    std::string can_if_name;
    std::string listen_address = "localhost";  // Default listen address
    std::string port = "50051";                // Default listen port

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
    auto broker_addr = getEnvVar("BROKER_ADDR", "localhost:55555");  // Replace hardcoded broker address and port

    Run(can_if_name, listen_address, port, broker_addr);

    return 0;
}
