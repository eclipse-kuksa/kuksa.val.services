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

#ifndef CANSIM_LIB_H
#define CANSIM_LIB_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "seatadjuster_engine.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>

// magic value used to for mocked sockfd
#define MOCKFD          1023 // anything > 1024 causes __fdelt_chk() abort with candump //0x0BADF00D

//// hooked libc original functions
typedef int (*socket_fn)     (int domain, int type, int protocol);
typedef int (*bind_fn)       (int fd, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*close_fn)      (int fd);
typedef unsigned int (*if_nametoindex_fn)( const char *ifname);
typedef ssize_t (*write_fn)  (int fd, const void *buf, size_t len);
typedef ssize_t (*read_fn)   (int fd, void *buf, size_t len);
typedef int (*ioctl_fn)      (int fd, unsigned long request, ...); // this causes buffer overflow
typedef int (*setsockopt_fn) (int fd, int level, int optname, const void *optval, socklen_t optlen);


// hook function declarations
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int ioctl(int fd, unsigned long request, ...);
unsigned int if_nametoindex(const char *ifname);
ssize_t write(int fd, const void *buf, size_t len);
ssize_t read(int fd, void *buf, size_t len);
int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);
int close(int fd);

typedef struct {
    socket_fn socket;
    bind_fn   bind;
    ioctl_fn  ioctl;
    write_fn  write;
    read_fn   read;
    if_nametoindex_fn if_nametoindex;
    setsockopt_fn setsockopt;
    close_fn  close;
} hook_table_t;

typedef struct {
    pthread_mutex_t lock;
    int mock_socket;
    bool mock_active;
    sae_context_t sae;
} sim_context_t;

// simulator internals
int sim_init_context(sim_context_t *ctx);
int sim_alloc_fd(sim_context_t* ctx);
int sim_is_mocked_fd(sim_context_t* ctx, int fd);
int sim_close_fd(sim_context_t* ctx, int fd);

sim_context_t* sim_context();

// external sim callbacks:
//extern ssize_t sae_read_cb(int fd, void *buf, size_t len);
//extern ssize_t sae_write_cb(int fd, const void *buf, size_t len);

extern bool debug;
extern bool verbose;

#ifdef	__cplusplus
} // extern "C"
#endif


#endif // CANSIM_LIB_H