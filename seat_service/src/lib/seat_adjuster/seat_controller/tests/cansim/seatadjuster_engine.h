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

#ifndef SEATADJUSTER_ENG_H
#define SEATADJUSTER_ENG_H

#include <sys/types.h>

#include <stdbool.h> // bool, true, false

#include <linux/can.h> // can_frame
#include <string.h>    // memset
#include <unistd.h>    // usleep

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

// If defined, all motors are dumped (potentially also simulated in future)
#define SAE_ALL_MOTORS


#define SAE_POS_INVALID -1//0xFF // MOTOR_POS_INVALID

// maps: MotorDirection enum
enum { MotorDirection_OFF=0, MotorDirection_DEC=1, MotorDirection_INC=2, MotorDirection_INV=3 };

/**
 * @brief maps LearningState enum
 */
enum { MotorLRN_NOK=0, MotorLRN_OK=1, MotorLRN_INV=2 };

extern bool _sae_debug;   // debug dumps from sim loop
extern bool _sae_verbose; // verbose dumps from sim loop

/**
 * @brief Seat Adjuster Engine Context
 */
typedef struct {
    int  _sim_fd; // file descriptor mapped to this context
    bool _sim_active; // disable read/write handling
    int  _sim_delay; // read delay in ms

    int  _sim_motor1_pos; // simulated motor1_pos [0..100] (fixed point)
    int  _sim_motor1_rpm; // simulated motor1_rpm/100: [30..254]
    int  _sim_motor1_lrn; // simulated motor1_lrn []
    int  _sim_motor1_status; // simulated motor1_status, based on write

#ifdef SAE_ALL_MOTORS
    int  _sim_motor2_pos; // simulated motor2_pos [0..100] (fixed point)
    int  _sim_motor2_rpm; // simulated motor2_rpm/100: [30..254]
    int  _sim_motor2_lrn; // simulated motor2_lrn []
    int  _sim_motor2_status; // simulated motor2_status, based on write

    int  _sim_motor3_pos; // simulated motor3_pos [0..100] (fixed point)
    int  _sim_motor3_rpm; // simulated motor3_rpm/100: [30..254]
    int  _sim_motor3_lrn; // simulated motor3_lrn []
    int  _sim_motor3_status; // simulated motor1_status, based on write

    int  _sim_motor4_pos; // simulated motor4_pos [0..100] (fixed point)
    int  _sim_motor4_rpm; // simulated motor4_rpm/100: [30..254]
    int  _sim_motor4_lrn; // simulated motor4_lrn []
    int  _sim_motor4_status; // simulated motor4_status, based on write
#endif // #SAE_ALL_MOTORS

    bool _sim_threshold_enabled; // enable/disable stopping at thresholds
    bool _sim_motor1_threshold_hi_stop; // true if stopped at high threshold
    bool _sim_motor1_threshold_lo_stop; // true if stopped at low threshold
    int  _sim_motor1_oldpos; // used to reduce cyclic dumps

    int64_t _sim_motor1_ts; // timestamp of last move start operation
    int  _sim_motor1_inc; // fractional increase per read tick
} sae_context_t;

/**
 * @brief Initialize SAE context structure with default values (also applyes "SAE_*" overrides from environment)
 *
 * @param ctx SAE context pointer
 * @return int 0 on success.
 */
int sae_init(sae_context_t *ctx);

/**
 * @brief Starts SAE simulation of file descriptor
 *
 * @param ctx SAE context pointer
 * @param fd Opened file descriptor for simulation
 * @return int 0 on sucess.
 */
int sae_start(sae_context_t *ctx, int fd);

/**
 * @brief SAE SocketCAN write callback
 *
 * @param ctx SAE context pointer
 * @param buf socketcan buffer (can_frame) from hooked write() call
 * @param len must be sizeof(struct can_frame)
 * @return ssize_t write result from callback (=len if successfull).
 */
ssize_t sae_write_cb(sae_context_t *ctx, const void *buf, size_t len);


/**
 * @brief SAE SocketCAN read callback
 *
 * @param ctx SAE context pointer
 * @param buf socketcan buffer (can_frame) from hooked read() call
 * @param len must be sizeof(struct can_frame)
 * @return ssize_t read result from callback (=len if successfull).
 */
ssize_t sae_read_cb(sae_context_t *ctx, void *buf, size_t len);

/**
 * @brief Estimates total time for full motor move based on provided RPM
 *
 * @param rpm motor1 rpm to estimate
 * @return int ms needed for full motor move
 */
int sae_estimate_move_time(int rpm);

/**
 * @brief Stops SAE simulation on specified context
 *
 * @param ctx SAE context pointer
 * @return int 0 on success.
 */
int sae_close(sae_context_t *ctx);

#endif