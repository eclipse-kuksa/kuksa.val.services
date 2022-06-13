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

#include <dlfcn.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <linux/can.h>
#include <net/if.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "cansim_lib.h"

#include "seatadjuster_engine.h"

// comment if ioctl() hook is causing memory corruption
#define HOOK_IOCTL

#define SOCKET_INVALID  -1

#define LIBC          "[libc] " // prefix for tracing system calls (verbose)
#define MOCK          "<MOCK> " // prefix for mocked calls
#define MODULE        "[CanSim]"
#define SELF_INIT       MODULE"[init] "
#define SELF_SOCKET     MODULE"[socket] "
#define SELF_BIND       MODULE"[bind] "
#define SELF_IOCTL      MODULE"[ioctl] "
#define SELF_CLOSE      MODULE"[close] "
#define SELF_READ       MODULE"[read] "
#define SELF_WRITE      MODULE"[write] "

#define SELF_ALLOC_FD   MODULE"[alloc_fd] "
#define SELF_CLOSE_FD   MODULE"[close_fd] "

static bool sim_initialized = false;

bool debug = true;
bool verbose = false;

// log to stderr by default
FILE* sim_log = NULL; // or fopen("sim.log")

static hook_table_t hook;
static sim_context_t sim;

// private function declarations
void _initialize();
void fprintf_hex(FILE* fp, const void* buf, ssize_t len);

void fprintf_hex(FILE* fp, const void* buf, ssize_t len) {
    for (int i = 0; len > 0 && i < len; i++) {
        fprintf(fp, "%02X", ((unsigned char*)buf)[i]);
    }
}

sim_context_t* sim_context() {
    if (!sim_initialized) {
        _initialize();
    }
    return &sim;
}

int sim_init_context(sim_context_t *ctx) {
    int res;
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(sim_context_t));
    ctx->mock_socket = SOCKET_INVALID;
    ctx->mock_active = false;
    res = pthread_mutex_init(&ctx->lock, NULL);
    if (res != 0) {
        fprintf(sim_log, SELF_INIT "Mutex initializaton failed: %d\n", res);
        // perror("Error initializing mutex: ");
        return -1;
    }
    res = sae_init(&ctx->sae);
    return res;
}

/**
 * @brief Shared library entry point. Hooks socket related system calls.
 *
 * Exits the process on error.
 */
void _initialize(void) {
    void* handle;

    // allow re-init of cansim_log, debug, etc.
    debug = getenv("CANSIM_DEBUG") &&  !strcmp(getenv("CANSIM_DEBUG"), "1");
    verbose = getenv("CANSIM_VERBOSE") && !strcmp(getenv("CANSIM_VERBOSE"), "1");
    const char *logname = getenv("CANSIM_LOG");
    if (logname) {
        fprintf(stderr, SELF_INIT "Logging to: %s\n", logname);
        sim_log = fopen(logname, "w");
    }
    if (sim_log == NULL) {
        // fallback to stderr
        sim_log = stderr;
    }

    if (sim_initialized) {
        fprintf(sim_log, SELF_INIT "Already Initialized!\n");
        return;
    }
    fprintf(sim_log, SELF_INIT "Initializing...\n");

    if (sim_init_context(&sim) != 0) {
        fprintf(sim_log, SELF_INIT "Init failed!\n");
        exit(1);
    }

    handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) {
        fprintf(sim_log, SELF_INIT "dlopen('libc.so.6') failed: %s\n", dlerror());
        exit(1);
    }
    // *(void **) - hack for pedantic C..
    *(void **)(&hook.socket) = dlsym(handle, "socket");
    if (!hook.socket) {
        fprintf(sim_log, SELF_INIT "hooking socket() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.bind) = dlsym(handle, "bind");
    if (!hook.bind) {
        fprintf(sim_log, SELF_INIT "hooking bind() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.ioctl) = dlsym(handle, "ioctl");
    if (!hook.ioctl) {
        fprintf(sim_log, SELF_INIT "hooking ioctl() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.write) = dlsym(handle, "write");
    if (!hook.write) {
        fprintf(sim_log, SELF_INIT "hooking write() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.read) = dlsym(handle, "read");
    if (!hook.read) {
        fprintf(sim_log, SELF_INIT "hooking read() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.if_nametoindex) = dlsym(handle, "if_nametoindex");
    if (!hook.if_nametoindex) {
        fprintf(sim_log, SELF_INIT "hooking if_nametoindex() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.setsockopt) = dlsym(handle, "setsockopt");
    if (!hook.setsockopt) {
        fprintf(sim_log, SELF_INIT "hooking setsockopt() failed: %s\n", dlerror());
        exit(1);
    }
    *(void **)(&hook.close) = dlsym(handle, "close");
    if (!hook.close) {
        fprintf(sim_log, SELF_INIT "hooking close() failed: %s\n", dlerror());
        exit(1);
    }
    // dlclose(handle);
    fprintf(sim_log, SELF_INIT "Initialized sucessfully.");
    fprintf(sim_log, "WARNING: Hooked libc socket(),bind(),read(),write(),ioctl(),setsockopt(),close() ...\n");

    sim_initialized = true;
}

void __attribute__ ((constructor)) sim_init(void) {
    fprintf(stderr, MODULE " Library Loaded\n"); // sim_log not initialized yet
    _initialize();
}

void __attribute__ ((destructor)) sim_fini(void) {
    sim.mock_active = false;
    // TODO: wait for blocked threads?
    if (sim_log) {
        fflush(sim_log);
        if (sim_log != stderr) {
            fclose(sim_log);
            sim_log = stderr; // don't crash on leaked refs
        }
    }
    fprintf(stderr, MODULE " Library Unloaded\n");
}


/**
 * @brief Returns if file decsriptor is mocked
 *
 * @param fd file descriptor from system call
 * @return boolean if fd matches current mocked descriptors.
 */
int sim_is_mocked_fd(sim_context_t* ctx, int fd) {
    if (!ctx) {
        if (verbose) fprintf(sim_log, MODULE " is_mocked_socket() called with invalid context!\n");
        return 0;
    }
    return fd == ctx->mock_socket && ctx->mock_active;
}

int sim_alloc_fd(sim_context_t* ctx) {
    int fd;
    // for now only 1 open can socket is supported
    if (!ctx) {
        if (verbose) fprintf(sim_log, SELF_ALLOC_FD "invalid context!\n");
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&ctx->lock);
    if (ctx->mock_socket == SOCKET_INVALID) {
        fd = MOCKFD; // or create dummy socket -> socket(AF_INET, SOCK_STREAM, 0);
        ctx->mock_socket = fd;
        ctx->mock_active = true;
        sae_init(&ctx->sae);
        sae_start(&ctx->sae, fd);
        errno = 0;
    } else {
        if (verbose) fprintf(sim_log, SELF_ALLOC_FD "mock_socked already allocated: %d!\n", ctx->mock_socket);
        fd = -1;
        errno = ENODEV;
    }
    pthread_mutex_unlock(&ctx->lock);
    return fd;
}

int sim_close_fd(sim_context_t* ctx, int fd) {
    // for now only 1 open can socket is supported
    if (!ctx) {
        if (verbose) fprintf(sim_log, SELF_CLOSE_FD "invalid context!\n");
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&ctx->lock);
    if (ctx->mock_socket == fd) {
        sae_close(&ctx->sae);
        ctx->mock_socket = SOCKET_INVALID;
        errno = 0;
    } else {
        if (verbose) fprintf(sim_log, SELF_CLOSE_FD "closing %d, but mock_socked is %d!\n", fd, ctx->mock_socket);
        errno = ENOTSOCK;
        return -1;
    }
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}


///////////////////////////////////////////////////////////////////////
//                       hooked libc functions                       //
///////////////////////////////////////////////////////////////////////

int socket(int domain, int type, int protocol) {
    int fd;
    if (domain == PF_CAN && type == SOCK_RAW && protocol == CAN_RAW) {
        fd = sim_alloc_fd(&sim);
        if (debug || verbose) {
            int errno__ = errno; // keep errno value
            fprintf(sim_log, MOCK "socket(%d, %d, %d) -> %d\n", domain, type, protocol, fd);
            errno = errno__;
        }
        return fd;
    }
    fd = hook.socket(domain, type, protocol);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "socket(%d, %d, %d) -> %d\n", domain, type, protocol, fd);
        errno = errno__;
    }

    return fd;
}

int bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    int ret;
    if (sim_is_mocked_fd(&sim, fd)) {
        int errno__;
        if (addrlen != sizeof(struct sockaddr_can)) {
            ret = -1;
            errno__ = EINVAL;
            fprintf(sim_log, MOCK "bind(%d, %p, %u) -> %d (invalid address length)\n", fd, (void*)addr, addrlen, ret);
        } else {
            struct sockaddr_can *can_addr = (struct sockaddr_can *)addr;
            ret = 0;
            if (debug || verbose) {
                fprintf(sim_log, MOCK "bind(%d, {family:%d, ifindex:%d}, %u) -> %d\n", fd, can_addr->can_family, can_addr->can_ifindex, addrlen, ret);
                // TODO: re-apply env variables?
            }
            errno__ = 0;
        }
        errno = errno__;
        return ret;
    }
    ret = hook.bind(fd, addr, addrlen);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "bind(%d, %p, %u) -> %d\n", fd, (void*)addr, addrlen, ret);
        errno = errno__;
    }
    return ret;
}

#ifdef HOOK_IOCTL
int ioctl(int fd, unsigned long request, ...) {
    int ret = -1;
    va_list valist;
    va_start(valist, request);
    char* arg = va_arg(valist, char*);

    // always fallback SIOCGIFINDEX to mocked as some tools use random socket for querying
    if (request == SIOCGIFINDEX || sim_is_mocked_fd(&sim, fd)) {
        if (request == SIOCGIFINDEX) {
            struct ifreq *ifr = (struct ifreq *)arg;
            if (ifr && ifr->ifr_name) {
                ret = 0;
                ifr->ifr_ifindex = 0;
                if (debug || verbose) {
                    fprintf(sim_log, MOCK "ioctl(%d, SIOCGIFINDEX, {name:'%s', idx:%d}) -> %d\n", fd, ifr->ifr_name, ifr->ifr_ifindex, ret);
                }
                return ret;
            }
        } else { // implies: sim_is_mocked_fd()
            // TODO: check if we should handle other ioctl() requests on mocked socket
            ret = 0;
            if (debug || verbose) {
                fprintf(sim_log, MOCK "ioctl(%d, %lu, %p) -> %d (Unhandled request)\n", fd, request, arg, ret);
            }
            return ret;
        }
    }
    ret = hook.ioctl(fd, request, arg);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "ioctl(%d, %lu, %p) -> %d\n", fd, request, (void*)arg, ret);
        errno = errno__;
    }
    va_end(valist);
    return ret;
}
#endif

ssize_t write(int fd, const void *buf, size_t len) {
    int ret;
    if (sim_is_mocked_fd(&sim, fd)) {
        int errno__ = 0;
        if (len == sizeof(struct can_frame)) {
            // additional consistency check
            if (sim.sae._sim_fd != fd) {
                fprintf(sim_log, MOCK "ERR: write(%d) called, but sae.fd is: %d\n", fd, sim.sae._sim_fd);
                ret = -1;
                errno__ = ENODEV;
            } else {
                if (verbose) {
                    fprintf(sim_log, MOCK "  >> sae_write_cb(%p, %p, %lu)\n", (void*)&sim.sae, buf, len);
                }
                ret = sae_write_cb(&sim.sae, buf, len);
                errno__ = errno; // get it from callback
            }
        } else {
            ret = -1;
            errno__ = ENOBUFS;
        }
        if (verbose || debug) {
            fprintf(sim_log, MOCK "write(%d, %p, %lu) -> %d\n", fd, buf, len, ret);
        }
        if (verbose) {
            fprintf(sim_log, MOCK "      tx-buf: 0x[");
            fprintf_hex(sim_log, buf, len);
            fprintf(sim_log, "]\n");
        }

        errno = errno__;
        return ret;
    }
    ret = hook.write(fd, buf, len);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "write(%d, %p, %lu) -> %d\n", fd, buf, len, ret);
        errno = errno__;
    }
    return ret;
}

ssize_t read(int fd, void *buf, size_t len) {
    int ret;
    if (sim_is_mocked_fd(&sim, fd)) {
        int errno__ = 0;
        if (len == sizeof(struct can_frame)) {
            // additional consistency check
            if (sim.sae._sim_fd != fd) {
                fprintf(sim_log, MOCK "ERR: read(%d) called, but sae.fd is: %d\n", fd, sim.sae._sim_fd);
                ret = -1;
                errno__ = ENODEV;
            } else {
                if (verbose) {
                    fprintf(sim_log, MOCK "  >> sae_read_cb(%p, %p, %lu)\n", (void*)&sim.sae, buf, len);
                }
                ret = sae_read_cb(&sim.sae, buf, len);
                errno__ = errno; // get it from callback
            }
        } else {
            ret = -1;
            errno__ = ENOBUFS;
        }
        if (verbose || debug) {
            fprintf(sim_log, MOCK "read(%d, %p, %lu) -> %d\n", fd, buf, len, ret);
        }
        if (verbose) {
            fprintf(sim_log, MOCK "      rx-buf: 0x[");
            fprintf_hex(sim_log, buf, len);
            fprintf(sim_log, "]\n");
        }

        errno = errno__;
        return ret;
    }
    ret = hook.read(fd, buf, len);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "read(%d, %p, %lu) -> %d\n", fd, buf, len, ret);
        errno = errno__;
    }
    return ret;
}

#if 1
unsigned int if_nametoindex(const char *ifname) {
    unsigned int ret = 0;
    if (!ifname) {
        errno = EINVAL;
        return -1;
    }
    if (strlen(ifname) > 3 && strstr(ifname, "can") == ifname) {
        ret = 42;
        if (debug || verbose) fprintf(sim_log, MOCK "if_nametoindex(%s) -> %u\n", ifname, ret);
        return ret;
    }
    if (strlen(ifname) > 4 && strstr(ifname, "vcan") == ifname) {
        ret = 43;
        if (debug || verbose) fprintf(sim_log, MOCK "if_nametoindex(%s) -> %u\n", ifname, ret);
        return ret;
    }
    ret = hook.if_nametoindex(ifname);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "if_nametoindex(%s) -> %u\n", ifname, ret);
        errno = errno__;
    }
    return ret;
}
#endif

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    int ret;
    if (sim_is_mocked_fd(&sim, fd)) {
        ret = 0;
        if (level == SOL_SOCKET && optname == SO_RCVTIMEO && optlen == sizeof(struct timeval)) {
            if (debug || verbose) {
                const struct timeval* tv = optval;
                fprintf(sim_log, MOCK "setsockopt(%d, SOL_SOCKET, SO_RCVTIMEO, tv:{%ld.%ld}) -> %d\n",
                        fd, tv->tv_sec, tv->tv_usec, ret);
            }
        }
        return ret;
    }
    ret = hook.setsockopt(fd, level, optname, optval, optlen);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "setsockopt(%d, %d, %d, %p, %u) -> %d\n",
                fd, level, optname, (void*)optval, optlen, ret);
        errno = errno__;
    }
    return ret;
}

int close(int fd) {
    int ret;
    if (sim_is_mocked_fd(&sim, fd)) { // fallback to mocked constat, as close(MOCKFD) will CRASH!
        ret = 0;
        if (debug || verbose) {
            fprintf(sim_log, MOCK "close(%d) -> %d\n", fd, ret);
        }
        sim_close_fd(&sim, fd);
        return ret;
    }
    ret = hook.close(fd);
    if (verbose) {
        int errno__ = errno;
        fprintf(sim_log, LIBC "close(%d) -> %d\n", fd, ret);
        errno = errno__;
    }
    return ret;
}