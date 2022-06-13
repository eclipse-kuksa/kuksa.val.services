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
 * @file      mock_unix_socket.h
 * @brief     File contains 
 */
#pragma once

#include <string>
#include <thread>

namespace sdv {
namespace test {

/**
 * @brief 
 * 
 */
class SocketMock {

public:
    SocketMock(const std::string &sock_name, bool debug = false);
    virtual ~SocketMock();
    int getSocket() const;

private:
    int newServerSocket();
    int newClientSocket();
    void serverThread();

private:
    int _serv_sock;
    int _client_socket;
    bool sock_running;
    std::thread *sock_thread;
    std::string _sock_name;
    bool debug;
};


}  // namespace test
}  // namespace sdv
