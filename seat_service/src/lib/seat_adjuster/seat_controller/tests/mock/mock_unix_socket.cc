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
 * @file      mock_unix_socket.cc
 * @brief     File contains 
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "mock_unix_socket.h"


namespace sdv {
namespace test {

/**
 * @brief 
 * 
 */
#ifndef SOCKET_INVALID
#  define SOCKET_INVALID -1
#endif

/**
 * @brief Construct a new Socket Mock object for specified Unix socket file.
 *
 * @param sock_name Opens server and client sockets running on specified sock_name Unix socket
 * @param debug enable verbose dumps from this object, can be overriden with ```export SOCKETMOCK_DEBUG=1```
 */
SocketMock::SocketMock(const std::string &sock_name, bool debug) :
    _serv_sock(SOCKET_INVALID),
    _client_socket(SOCKET_INVALID),
    sock_running(false),
    sock_thread(nullptr),
    _sock_name(sock_name),
    debug(debug)
{
    #define SELF   "[SocketMock::SocketMock] "

    // override verbose with env
    if (::getenv("SOCKETMOCK_DEBUG")) {
        debug = ::atoi(::getenv("SOCKETMOCK_DEBUG"));
    }
    int rc = newServerSocket();
    if (rc == -1) {
        printf(SELF "### Failed to create server socket: %s\n", _sock_name.c_str());
        return;
    }
    int fd = newClientSocket();
    if (fd == -1) {
        printf(SELF "### Failed to connect client socket: %s\n", _sock_name.c_str());
        return;
    }

    printf(SELF "[%s] --> { server_sock:%d, client_sock:%d }\n", _sock_name.c_str(), _serv_sock, _client_socket);
    #undef SELF
}

/**
 * @brief Returns client socket that can handle read() and write() calls from socketcan code.
 *
 * @return int client socket descriptor (to be mocked in seatctrl_context->socket)
 */
int SocketMock::getSocket() const {
    return _client_socket;
}

/**
 * @brief 
 * 
 */
SocketMock::~SocketMock() {
    #define SELF   "[SocketMock::~SocketMock] "
    sock_running = false;
    // FIXME client socket should be closed by client?
    if (_client_socket != SOCKET_INVALID) {
        if (debug) printf(SELF "### closing client socket: %d\n", _client_socket);
        ::close(_client_socket);
        _client_socket = SOCKET_INVALID;
    }
    if (_serv_sock != SOCKET_INVALID) {
        if (debug) printf(SELF "### closing server socket: %d\n", _client_socket);
        ::close(_serv_sock);
        _serv_sock = SOCKET_INVALID;
    }
    if (sock_thread != nullptr) {
        if (debug) printf(SELF "### waiting for thread...\n");
        if (sock_thread->joinable()) {
            this->sock_thread->join();
        }
        delete sock_thread;
        sock_thread = nullptr;
    }
    #undef SELF
}

/**
 * @brief Allocates server Unix socket on specified location
 *
 * @return int server socket fd
 */
int SocketMock::newClientSocket() {
    #define SELF   "[SocketMock::newClientSocket] "

    int rc = -1;
    int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (debug) printf(SELF "### Opened client Unix socket: %d\n", sock);
    if (sock == -1) {
        perror(SELF "Failed creating Unix socket:");
        return SOCKET_INVALID;
    }
    struct sockaddr_un serv_addr;
    ::memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    ::strncpy(serv_addr.sun_path, _sock_name.c_str(), sizeof(serv_addr.sun_path)-1);

    printf(SELF "### Connecting to: %s\n", serv_addr.sun_path);
    rc = ::connect(sock, (struct sockaddr*) &serv_addr, SUN_LEN(&serv_addr));
    if (rc == -1) {
        perror(SELF "Connecting to Unix socket failed:");
    }
    if (rc == 0) {
        _client_socket = sock;
    }
    return rc;

    #undef SELF
}

/**
 * @brief Allocates server Unix socket on specified location
 *
 * @return int server socket fd
 */
int SocketMock::newServerSocket() {
    #define SELF "[SocketMock::newServerSocket] "
    int sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
    printf(SELF "### Opened server Unix socket: %d\n", sock);
    if (sock == -1) {
        perror(SELF "Failed creating Unix socket:");
        return SOCKET_INVALID;
    }

    // cleanup unix socket
    ::unlink(_sock_name.c_str());

    struct sockaddr_un serv_addr;
    ::memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;
    ::strncpy(serv_addr.sun_path, _sock_name.c_str(), sizeof(serv_addr.sun_path)-1);

    printf(SELF "### Binding to: %s\n", serv_addr.sun_path);
    if (::bind(sock, (struct sockaddr *)&serv_addr, SUN_LEN(&serv_addr)) < 0) {
        perror(SELF "Failed binding socket:");
        ::close(sock);
        return SOCKET_INVALID;
    }

    printf(SELF "### thread listening on: %d\n", sock);
    if (::listen(sock, 1) < 0) {
        perror(SELF "Failed listening on Unix socket:");
        ::close(sock);
        return SOCKET_INVALID;
    }

    struct timeval tv;
    tv.tv_sec = 1; // 1 sec timeout
    tv.tv_usec = 0;
    if (::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof tv)) {
        perror(SELF "Failed setsockopt(SO_RCVTIMEO) on socket:");
    }

    this->_serv_sock = sock;
    this->sock_thread = new std::thread(&SocketMock::serverThread, this);

    int retry = 0;
    while (!this->sock_running && ++retry < 10) {
        ::usleep(100*1000L);
    }
    if (this->sock_running) {
        if (debug) printf(SELF "### Thread Started.\n");
    } else {
        printf(SELF "### Thread Aborted.\n");
        this->sock_thread->join();
        ::close(this->_serv_sock);
        this->_serv_sock = SOCKET_INVALID;
        return SOCKET_INVALID;
    }

    return sock;

    #undef SELF
}
/**
 * @brief Creates server Thread
 *
 * 
 */
void SocketMock::serverThread() {
    #define SELF "[SocketMock::serverThread] "
    sockaddr cli_addr;
    socklen_t cli_len;

    this->sock_running = true;
    while (this->sock_running) {
        ::usleep(1000L);
        printf(SELF "### Accepting on server_sock: %d\n", _serv_sock);

        ::memset(&cli_addr, 0, sizeof(sockaddr));
        cli_len = sizeof(cli_addr);
        int clientfd = ::accept(_serv_sock, &cli_addr, &cli_len);
        if (clientfd == -1) {
            if (errno != EINTR) {
                perror(SELF "Failed accepting client");
            }
            usleep(100 * 1000L);
            continue;
        }

        while (this->sock_running && _serv_sock != -1) {
            if (debug) printf(SELF " --- reading from client: %d\n", clientfd);
            struct can_frame cf;
            ::memset(&cf, 0, sizeof(cf));
            int rc = ::read(clientfd, &cf, sizeof (cf));
            if (!this->sock_running) break; // avoid perrors on close()
            if (rc == -1) {
                perror(SELF "Failed reading from client");
                // TODO: close(clientfd);
                break;
            }
            if (debug) printf(SELF " --> read(): %d\n", rc);
            if (rc == sizeof(cf)) {
                // this is optional?
                if (debug) printf(SELF " --- writing to client: %d\n", clientfd);
                rc = ::write(clientfd, &cf, sizeof (cf));
                if (!this->sock_running) break; // avoid perrors on close()
                if (rc == -1) {
                    perror(SELF "Failed writing to client");
                    // TODO: close(clientfd);
                    break;
                }
                if (debug) printf(SELF " --> write(): %d\n", rc);
            }
            usleep(1000L);
        }
        if (debug) printf(SELF "### closing client: %d\n", clientfd);
        close(clientfd);
    }
}

}  // namespace test
}  // namespace sdv
