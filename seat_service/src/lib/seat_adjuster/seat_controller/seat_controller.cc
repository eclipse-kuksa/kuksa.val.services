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
 * @file      seat_controller.cc
 * @brief     File contains seat controller implementation for SeatAdjuster ECU
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <pthread.h>

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>

// cantools generated code from .dbc
#include "CAN.h"

#include "seat_controller.h"


//// function dump prefix ////

#define PREFIX_CAN      "        [CAN]: "
#define PREFIX_CTL      "   [CTL Loop]: "
#define PREFIX_STAT     " [SECU1_STAT]: "
#define PREFIX_APP      "[SeatCtrl:"
#define SELF_INIT         PREFIX_APP ":init_ctx] "
#define SELF_OPEN         PREFIX_APP ":open] "
#define SELF_CLOSE        PREFIX_APP ":close] "
#define SELF_CMD1         PREFIX_APP ":_send_cmd] "
#define SELF_STOPMOV      PREFIX_APP ":stop_move] "
#define SELF_SETPOS       PREFIX_APP ":set_position] "
#define SELF_SETPOS_CB    PREFIX_APP ":set_pos_cb] "


//////////////////////////
// private declarations //
//////////////////////////

int64_t get_ts();
const char* mov_state_string(int dir);

void print_secu1_cmd_1(const char* prefix, CAN_secu1_cmd_1_t *cmd);
void print_secu1_stat(const char* prefix, CAN_secu1_stat_t *stat);
void print_can_raw(const struct can_frame *frame, bool is_received);
void print_ctl_stats(seatctrl_context_t *ctx, const char* prefix);

error_t handle_secu_stat(seatctrl_context_t *ctx, const struct can_frame *frame);
error_t seatctrl_send_cmd1(seatctrl_context_t *ctx, uint8_t motor1_dir, uint8_t motor1_rpm);
error_t seatctrl_controll_loop(seatctrl_context_t *ctx);


/**
 * @brief Prints RAW can_frame on stdout
 *
 * @param frame
 * @param is_received
 */
void print_can_raw(const struct can_frame *frame, bool is_received)
{
    char buf[256];
    snprintf(buf, sizeof(buf), PREFIX_CAN "%s: 0x%03X [%d] ", (is_received ? "RX" : "TX" ), frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++)
        snprintf(buf + strlen(buf), sizeof(buf),"%02X ", frame->data[i]);
    printf("%s\n", buf);
}


/**
 * @brief Returns string description for MotorDirection enum values.
 *
 * @param dir
 * @return const char*
 */
const char* mov_state_string(int dir)
{
    switch (dir) {
        case MotorDirection::OFF: return "OFF";
        case MotorDirection::INC: return "INC";
        case MotorDirection::DEC: return "DEC";
        case MotorDirection::INV: return "INV";
        default: return "Undefined!";
    }
}


/**
 * @brief Describes CAN_secu1_stat_t.motorX_learning_state
 *
 * @param state
 * @return const char*
 */
const char* learning_state_string(int state)
{
    switch (state) {
        case CAN_SECU1_STAT_MOTOR1_LEARNING_STATE_NOT_LEARNED_CHOICE:
            return "NOK";
        case CAN_SECU1_STAT_MOTOR1_LEARNING_STATE_LEARNED_CHOICE:
            return "OK";
        case CAN_SECU1_STAT_MOTOR1_LEARNING_STATE_INVALID_CHOICE:
            return "INV";
        default:
            return "Undefined!";
    }
}


/**
 * @brief Helper for dumping hex bytes
 *
 * @param prefix
 * @param buf
 * @param len
 */
static void dumphex(const char* prefix, const void *buf, ssize_t len) {
    printf("%s <%ld> [", prefix, len);
    for (int i = 0; len > 0 && i < len; i++) {
        printf("%02X ", ((uint8_t*)buf)[i]);
    }
    printf("]\n");
}


/**
 * @brief Prints CTL stats (active command, desired position, etc) on stdout
 *
 * @param ctx SeatCtrl context
 * @param prefix string to print before stats
 */
void print_ctl_stats(seatctrl_context_t *ctx, const char* prefix)
{
    int64_t elapsed = ctx->command_ts != 0 ? get_ts() - ctx->command_ts : -1;
    printf("%smotor1:{ pos:%3d%%, %-3s } --> target:{ pos:%3d%%, %3s }, elapsed: %" PRId64 " ms.\n",
            prefix,
            ctx->motor1_pos,
            mov_state_string(ctx->motor1_mov_state),
            ctx->desired_position,
            mov_state_string(ctx->desired_direction),
            elapsed
        );
}


/**
 * @brief Prints CAN_secu1_cmd_1_t* in human readable format to stdout
 *
 * @param prefix string to print before stats
 * @param cmd CAN_secu1_cmd_1_t*
 */
void print_secu1_cmd_1(const char* prefix, CAN_secu1_cmd_1_t *cmd)
{
#ifdef SEAT_CTRL_ALL_MOTORS // reduce extra dumps on console, we care about motor1 only
    printf("%s[SECU1]{ m1_cmd: %s, m1_rpm: %d, m2_cmd: %s, m2_rpm: %d, m3_cmd: %s, m3_rpm: %d, m4_cmd: %s, m4_rpm: %d }\n",
            prefix,
            mov_state_string(cmd->motor1_manual_cmd), cmd->motor1_set_rpm * 100,
            mov_state_string(cmd->motor2_manual_cmd), cmd->motor2_set_rpm * 100,
            mov_state_string(cmd->motor3_manual_cmd), cmd->motor3_set_rpm * 100,
            mov_state_string(cmd->motor4_manual_cmd), cmd->motor4_set_rpm * 100);
#else
    printf("%s[SECU1]{ motor1_cmd: %s, motor1_rpm: %d }\n",
            prefix,
            mov_state_string(cmd->motor1_manual_cmd), cmd->motor1_set_rpm * 100);
#endif
}


/**
 * @brief Prints CAN_secu1_stat_t* in human readable format to stdout
 *
 * @param prefix
 * @param stat
 */
void print_secu1_stat(const char* prefix, CAN_secu1_stat_t *stat)
{
#ifdef SEAT_CTRL_ALL_MOTORS
    printf("%s m1:{pos:%3d%%, mov: %-3s, lrn: %s} m2:{pos:%3d%%, mov: %-3s, lrn: %s} \n",
            prefix,
            stat->motor1_pos, // CAN_secu1_stat_motor1_pos_decode() - not generated if float code is disabled! Make sure scaling remains "default"!
            mov_state_string(stat->motor1_mov_state),
            learning_state_string(stat->motor1_learning_state),
            stat->motor2_pos,
            mov_state_string(stat->motor2_mov_state),
            learning_state_string(stat->motor2_learning_state)
        );
#else
    printf("%s{ motor1_pos:%3d%%, motor1_mov_state: %-3s, motor1_learning_state: %s }\n",
            prefix,
            stat->motor1_pos, // CAN_secu1_stat_motor1_pos_decode() - not generated if float code is disabled! Make sure scaling remains "default"!
            mov_state_string(stat->motor1_mov_state),
            learning_state_string(stat->motor1_learning_state)
        );
#endif
}


/**
 * @brief Handler function for processing SECUx_STAT commands
 *
 * @param ctx SeatCtrl context
 * @param frame can_frame with CanID = CAN_SECU1_STAT_FRAME_ID
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t handle_secu_stat(seatctrl_context_t *ctx, const struct can_frame *frame)
{
    CAN_secu1_stat_t stat;
    memset(&stat, 0, sizeof(CAN_secu1_stat_t));

    if (frame->can_id != CAN_SECU1_STAT_FRAME_ID) {
        printf(PREFIX_CTL "ERR: Not a CAN_SECU1_STAT_FRAME_ID frame! (%d)\n", frame->can_id);
        return SEAT_CTRL_ERR_INVALID;
    }
    int rc = CAN_secu1_stat_unpack(&stat, frame->data, frame->can_dlc);
    if (rc != 0) {
        printf(PREFIX_CTL "ERR: Failed unpacking CAN_SECU1_STAT_FRAME_ID frame!\n");
        return SEAT_CTRL_ERR;
    }

    // if values in range -> update motor1 last known pos. helpful against cangen attacks
    if (CAN_secu1_stat_motor1_pos_is_in_range(stat.motor1_pos) &&
        ((int)stat.motor1_pos <= 100 || (int)stat.motor1_pos == MOTOR_POS_INVALID) && // range always positive
        CAN_secu1_stat_motor1_mov_state_is_in_range(stat.motor1_mov_state) &&
        CAN_secu1_stat_motor1_learning_state_is_in_range(stat.motor1_learning_state))
    {
        if (ctx->config.debug_stats) {
            // dump unique?
            if (ctx->config.debug_verbose ||
                ctx->motor1_pos != stat.motor1_pos ||
                ctx->motor1_learning_state != stat.motor1_learning_state ||
                ctx->motor1_mov_state != stat.motor1_mov_state)
            {
                print_secu1_stat(PREFIX_STAT, &stat);
            }
        }

        if (ctx->running && ctx->event_cb != NULL && ctx->motor1_pos != stat.motor1_pos) {
            if (ctx->config.debug_verbose) printf(PREFIX_CTL " calling cb: %p(Motor1Pos, %d)\n", (void*)ctx->event_cb, stat.motor1_pos);
            ctx->event_cb(SeatCtrlEvent::Motor1Pos, stat.motor1_pos, ctx->event_cb_user_data);
        }

        ctx->motor1_mov_state = stat.motor1_mov_state;
        ctx->motor1_learning_state = stat.motor1_learning_state;
        ctx->motor1_pos = stat.motor1_pos; // decode?

        return SEAT_CTRL_OK;
    }

    return SEAT_CTRL_ERR_INVALID;
}

/**
 * @brief Gets current timestamp (monotonic).
 *
 * @return int64_t current stamp in milliseconds
 */
int64_t get_ts()
{
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return (int64_t)spec.tv_sec * 1000L + (int64_t)spec.tv_nsec / 1000000L;
}

/**
 * @brief Helper for checking if Control Loop (CTL) is running
 * (context is valid and there is pending move command)
 *
 * @param ctx SeatCtrl context
 * @return true if control loop is running (and motor is moving to desired position)
 */
bool is_ctl_running(seatctrl_context_t *ctx)
{
    if (ctx->socket != SOCKET_INVALID &&
        ctx->command_ts > 0 &&
        ctx->desired_direction != MotorDirection::OFF &&
        ctx->desired_position != MOTOR_POS_INVALID) {
        return true;
    }
    return false;
}

/**
 * @brief See seat_controller.h
 */
int seatctrl_get_position(seatctrl_context_t *ctx)
{
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf("[seatctrl_get_position] ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    if (!ctx->running) {
        return SEAT_CTRL_ERR; // CTR not yet started or stopping
    }
    return (int)ctx->motor1_pos; // Last position or MOTOR_POS_INVALID
}

// TODO: move in context
static int last_ctl_pos = MOTOR_POS_INVALID;
static int last_ctl_dir = 0;

static bool learned_mode = true;         // assume motor learned mode
static int64_t learned_mode_changed = 0; // rate limit state change dumps
#define LEARNED_MODE_RATE	10*1000L     // timeout (ms) to ignore dumps about learned state change


/**
 * @brief Handles Seat Adjustment Control Loop (CTL)
 *
 * @param ctx SeatCtrl context
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error
 */
error_t seatctrl_controll_loop(seatctrl_context_t *ctx)
{
    error_t rc = SEAT_CTRL_OK;
    // FIXME: Handle ctx->motor1_learning_state == LearningState::NotLearned
    if (learned_mode && ctx->motor1_learning_state == LearningState::NotLearned) {
        learned_mode = false;
        int ts = get_ts();
        // fix for alternating state change flood (probably caused by concurrent canoe instances on can0)
        if (ts - learned_mode_changed > LEARNED_MODE_RATE) {
            printf("\n");
            printf(PREFIX_CTL "WARN: *** ECU in not-learned state! Consider running: ./ecu-reset -s can0\n\n");
            fflush(stdout);
            learned_mode_changed = ts;
        }
    } else
    if (!learned_mode && ctx->motor1_learning_state == LearningState::Learned) {
        learned_mode = true;
        int ts = get_ts();
        if (ts - learned_mode_changed > LEARNED_MODE_RATE) {
            printf("\n");
            printf(PREFIX_CTL "*** ECU changed to: learned state!\n");
            fflush(stdout);
            learned_mode_changed = ts;
        }
    }
    //   In that state normalization loop must be done on real hw.
    if (is_ctl_running(ctx)) {
        int64_t elapsed = get_ts() - ctx->command_ts;
        // Preliminary phase: operation was just scheduled (up to 500ms ago),
        // but can signal may not yet come, i.e. waiting for motor tor start moving
        if (elapsed < 500 && ctx->motor1_mov_state == MotorDirection::OFF && ctx->motor1_pos != ctx->desired_position) {
            printf(PREFIX_CTL "* Seat Adjustment to (%d, %s) active, waiting motor movement for %" PRId64 "ms.\n",
                    ctx->desired_position,
                    mov_state_string(ctx->desired_direction),
                    elapsed);
            ::usleep(1000);
            return SEAT_CTRL_OK;
        }

        // reduce frequency of dumps, only if something relevant changed,
        // but don't cache states when command was just started (e.g. motor off warning will be dumped always)
        if (last_ctl_pos != ctx->motor1_pos || last_ctl_dir != ctx->motor1_mov_state) {
            if (ctx->config.debug_ctl) print_ctl_stats(ctx, PREFIX_CTL);
            if (ctx->motor1_mov_state != ctx->desired_direction && ctx->motor1_pos != ctx->desired_position) {
                printf("\n");
                printf(PREFIX_CTL "WARN: *** Seat Adjustment to (%d, %s) active, but motor1_mov_state is %s.\n",
                        ctx->desired_position,
                        mov_state_string(ctx->desired_direction),
                        mov_state_string(ctx->motor1_mov_state));
                // Workaround for possible "bug" in seat adjuster ECU that is stopping (OFF) at
                // some thresholds at both ends of the range (e.g. 14% and 80%)
                if (ctx->motor1_mov_state == MotorDirection::OFF) {
                    printf(PREFIX_CTL " >>> Sending MotorOff command...\n");
                    error_t rc0 = seatctrl_send_cmd1(ctx, MotorDirection::OFF, 0); // off, 0rpm
                    if (rc0 != SEAT_CTRL_OK) {
                        perror(PREFIX_CTL "seatctrl_send_cmd1(OFF) error");
                    }
                    ::usleep(100*1000L); // it needs some time to process the off command. TODO: check with ECU team
                    printf(PREFIX_CTL ">>> Re-sending: SECU1_CMD_1 [ motor1_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                            ctx->motor1_pos, ctx->desired_position, mov_state_string(ctx->desired_direction), ctx->command_ts);
                    seatctrl_send_cmd1(ctx, ctx->desired_direction, ctx->config.motor_rpm);
                    if (rc != SEAT_CTRL_OK) {
                        perror(PREFIX_CTL "seatctrl_send_cmd1(desired_pos) error");
                    }
                }
                printf("\n");
            }
            if (ctx->motor1_pos == MOTOR_POS_INVALID) {
                printf(PREFIX_CTL "WARN: *** Seat Adjustment to (%d, %s) active, but motor1_pos is: %d.\n",
                        ctx->desired_position,
                        mov_state_string(ctx->desired_direction),
                        ctx->motor1_pos);
                        // break; ?
            }
            last_ctl_dir = ctx->motor1_mov_state;
            last_ctl_pos = ctx->motor1_pos;
        }
        // FIXME: if desired_direction INC && ctx->desired_position >= ctx->motor1_pos
        if ( ctx->motor1_pos != MOTOR_POS_INVALID &&
             ((ctx->desired_direction == MotorDirection::INC && ctx->motor1_pos >= ctx->desired_position) ||
              (ctx->desired_direction == MotorDirection::DEC && ctx->motor1_pos <= ctx->desired_position) ))
        {
            // Terminal state, reached destination
            printf(PREFIX_CTL "*** Seat Adjustment (%d, %s) finished at pos: %d for %" PRId64 "ms.\n",
                    ctx->desired_position,
                    mov_state_string(ctx->desired_direction),
                    ctx->motor1_pos,
                    elapsed);
            seatctrl_stop_movement(ctx);
            // invalidate last states
            last_ctl_dir = 0;
            last_ctl_pos = MOTOR_POS_INVALID;
        } else
        if (elapsed > ctx->config.command_timeout) {
            // stop movement due to timeout
            printf(PREFIX_CTL "WARN: *** Seat adjustment to (%d, %s) timed out (%" PRId64 "ms). Stopping motors.\n",
                    ctx->desired_position,
                    mov_state_string(ctx->desired_direction),
                    elapsed);
            seatctrl_stop_movement(ctx);
            // invalidate last states
            last_ctl_dir = 0;
            last_ctl_pos = MOTOR_POS_INVALID;
        }
    }
    return rc;
}

/**
 * @brief Thread funcion running CTL
 *
 * @param arg
 * @return void*
 */
void *seatctrl_threadFunc(void *arg)
{
    seatctrl_context_t *ctx = (seatctrl_context_t *)arg;
    if (ctx->config.debug_verbose) printf(PREFIX_CTL "Thread started.\n");

    ctx->running = true;
    while (ctx->running && ctx->socket != SOCKET_INVALID)
    {
        struct can_frame frame;
        memset(&frame, 0, sizeof(frame));
        // FIXME: poll vs blocking read to allow termination in case of missing can frames!
        int cnt = read(ctx->socket, &frame, sizeof(struct can_frame));
        int err = errno;
        if (cnt < 0 && err == EAGAIN) {
            if (ctx->config.debug_verbose) printf(PREFIX_CAN "read() timeout\n");
            if (!ctx->running) {
                //if (ctx->config.debug_verbose)
                printf(PREFIX_CAN "CTL Loop terminating!\n");
                break;
            }
            ::usleep(1000 * 1000u);
            continue;
        }
        if (cnt < 0 && err == EINTR) { // BUGFIX: do not abort on EINTR
            if (ctx->config.debug_verbose) printf(PREFIX_CAN "read() interrupted\n");
            ::usleep(1 * 1000u);
            continue;
        }
        if (cnt < 0)
        {
            printf(PREFIX_CTL "read() -> %d, errno: %d\n", cnt, err);
            perror(PREFIX_CTL "SocketCan Read failed");
            ::usleep(1000 * 1000u);

            if (ctx->event_cb) {
                if (ctx->config.debug_verbose) printf(PREFIX_CTL " calling cb: %p(CanError, %d)\n", (void*)ctx->event_cb, err);
                ctx->event_cb(SeatCtrlEvent::CanError, err, ctx->event_cb_user_data);
            }

            // FIXME: decide should reading attempts continue on error? e.g. check good/bad errno values
            if (cnt == ENETDOWN) {
                continue; // know to be OK to recover when canX is up again
            } else {
                printf(PREFIX_CAN "CTL Loop terminating!\n");
                break; // other error conditions not tested
            }
        }

        if (ctx->config.debug_raw) {
             print_can_raw(&frame, true);
             if (ctx->config.debug_verbose) {
                 dumphex("RX-RAW ", &frame, sizeof(struct can_frame));
             }
        }
        // TODO: pthread_mutex lock in ctx
        if (frame.can_id == CAN_SECU1_STAT_FRAME_ID)
        {
            if (handle_secu_stat(ctx, &frame) == SEAT_CTRL_OK) {
                seatctrl_controll_loop(ctx);
            }
        }
        ::usleep(1000);
    }

    if (ctx->config.debug_verbose) printf(PREFIX_CTL "Thread stopped.\n");
    return NULL;
}


/**
 * @brief Sends an CAN_secu1_cmd_1_t to SocketCAN
 *
 * @param ctx SeatCtrl context
 * @param motor1_dir motor1 move direction: value of MotorDirection enum
 * @param motor1_rpm motor1 RPMs (actually PWM percentage in range [30-100] or 0 to stop movement)
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error
 */
error_t seatctrl_send_cmd1(seatctrl_context_t *ctx, uint8_t motor1_dir, uint8_t motor1_rpm)
{
    int rc;
    CAN_secu1_cmd_1_t cmd1;
    struct can_frame frame;

    if (ctx->socket == SOCKET_INVALID) {
        printf(SELF_CMD1 "ERR: CAN Socket not available!\n");
        return SEAT_CTRL_ERR;
    }
    // argument check?

    memset(&cmd1, 0, sizeof(CAN_secu1_cmd_1_t));
    // FIXME: range checks! [0..254]
    cmd1.motor1_manual_cmd = motor1_dir;
    cmd1.motor1_set_rpm = motor1_rpm;

    memset(&frame, 0, sizeof(struct can_frame));
    frame.can_id = CAN_SECU1_CMD_1_FRAME_ID;
    rc = CAN_secu1_cmd_1_pack(frame.data, &cmd1, sizeof(CAN_secu1_cmd_1_t));
    if (rc < 0) {
        printf(SELF_CMD1 "ERR: CAN_secu1_cmd_1_pack() error\n");
        return SEAT_CTRL_ERR;
    }
    frame.can_dlc = 8; // = rc; BUGFIX: we have to send full 8 bytes, regardless of actual CAN_secu1_cmd_1_pack packed size, append 00s
    print_secu1_cmd_1(SELF_CMD1 "*** Sending SECU1_CMD_1: " , &cmd1);
    if (ctx->config.debug_raw) {
        print_can_raw(&frame, false);
    }

    if (write(ctx->socket, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        int err = errno;
        perror(SELF_CMD1 "CAN Socket write failed");
        if (ctx->event_cb) {
            if (ctx->config.debug_verbose) printf(SELF_CMD1 " calling cb: %p(CanError, %d)\n", (void*)ctx->event_cb, err);
            ctx->event_cb(SeatCtrlEvent::CanError, err, ctx->event_cb_user_data);
        }
        return SEAT_CTRL_ERR_CAN_IO;
    }

    return SEAT_CTRL_OK;
}


/**
 * @brief See seat_controller.h
 */
error_t seatctrl_stop_movement(seatctrl_context_t *ctx)
{
    printf(SELF_STOPMOV "Sending MotorOff command...\n");
    error_t rc = seatctrl_send_cmd1(ctx, MotorDirection::OFF, 0); // off, 0rpm
    if (rc != SEAT_CTRL_OK) {
        perror(SELF_STOPMOV "seatctrl_send_cmd1() error");
        // also invalidate CTL?
    }

    // invalidate states. FIXME: lock with mutex?
    ctx->desired_position = MOTOR_POS_INVALID;
    ctx->desired_direction = MotorDirection::OFF;
    ctx->command_ts = 0;

    return rc;
}

/**
 * @brief See seat_controller.h
 */
error_t seatctrl_set_position(seatctrl_context_t *ctx, int32_t desired_position)
{
    error_t rc = 0;
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf(SELF_SETPOS "ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    // abort if current pos / directions are unknown
    printf("\n" SELF_SETPOS "Seat Adjustment requested for position: %d%%.\n", desired_position);
    if (desired_position < 0 || desired_position > 100) {
        printf(SELF_SETPOS "ERR: Invalid position!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    print_ctl_stats(ctx, SELF_SETPOS);

    // FIXME: use pthred_mutex in ctx?

    // sanity checks for incoming can signal states
    if (ctx->motor1_pos == MOTOR_POS_INVALID) {
        printf(SELF_SETPOS "WARN: Motor1 position is invalid: %d\n", ctx->motor1_pos);
        // wait some more and if still not incoming - bail out with error
        for (int retries = 0; retries < 30; retries++) { // wait up to 3 sec
            if (ctx->motor1_pos != MOTOR_POS_INVALID) {
                break;
            }
            usleep(100 * 1000L);
        }
        if (ctx->motor1_pos == MOTOR_POS_INVALID) {
            printf(SELF_SETPOS "Check %s interface for incoming SECU1_STAT frames!\n", ctx->config.can_device);
            printf(SELF_SETPOS "Seat Adjustment to %d%% aborted.\n", desired_position);
            return SEAT_CTRL_ERR_NO_FRAMES;
        }
    }
    if (ctx->motor1_mov_state != MotorDirection::OFF)
    {
        printf(SELF_SETPOS "WARN: Motor1 status is %s\n", mov_state_string(ctx->motor1_mov_state));
    }

    if (is_ctl_running(ctx) && ctx->desired_position != desired_position)
    {
        printf(SELF_SETPOS "WARN: Overriding previous motor1_pos[%d] with new value:[%d]\n", ctx->desired_position, desired_position);
    }
    // BUGFIX: always send motor off command
    rc = seatctrl_stop_movement(ctx);
    usleep(100 * 1000L);
    //if (ctx->desired_position != MOTOR_POS_INVALID && ctx->desired_position != desired_position || ctx->motor1_mov_state != MotorDirection::OFF)

    int current_pos = ctx->motor1_pos;
    if (current_pos == desired_position) {
        printf(SELF_SETPOS "*** Already at requested position: %d%%\n", desired_position);
        if (ctx->motor1_mov_state != MotorDirection::OFF) {
            rc = seatctrl_stop_movement(ctx);
        } else {
            ctx->desired_direction = MotorDirection::OFF;
            ctx->desired_position = MOTOR_POS_INVALID;
            ctx->command_ts = 0;
        }
        return SEAT_CTRL_OK;
    }

    // calculate desired direction based on last known position

    MotorDirection direction = MotorDirection::INV;
    if (current_pos < desired_position) {
        direction = MotorDirection::INC;
    } else {
        direction = MotorDirection::DEC;
    }
    // sync!
    ctx->command_ts = get_ts();
    ctx->desired_direction = direction;
    ctx->desired_position = desired_position;
    // FIXME: SECUx_CMD1 Movement Status has the same values as SECUX
    printf(SELF_SETPOS "Sending: SECU1_CMD_1 [ motor1_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
            ctx->motor1_pos, ctx->desired_position, mov_state_string(direction), ctx->command_ts);

    rc = seatctrl_send_cmd1(ctx, direction, ctx->config.motor_rpm);
    if (rc < 0) {
        perror(SELF_SETPOS "seatctrl_send_cmd1() error");
        // FIXME: abort operation
        return rc;
    }

#if 0 // EXPERIMENTAL
    // TODO: evaluate ctx->motor1_pos and states to check direction
    for (int i=0; i<500; i++) {
        usleep(1*1000L); // give motor some time to start moving, then set ctl_active flag
        if (ctx->motor1_mov_state != MotorDirection::OFF) {
            int64_t elapsed = ctx->command_ts != 0 ? get_ts() - ctx->command_ts : -1;
            printf(SELF_SETPOS "Motor movement detected [ motor_pos: %d%%, dir: %s ] in %" PRId64 "ms.\n",
                ctx->motor1_pos, mov_state_string(ctx->motor1_mov_state), elapsed);
            break;
        }
    }
#endif
    return rc;
}


/**
 * @brief See seat_controller.h
 */
error_t seatctrl_default_config(seatctrl_config_t *config)
{
    if (!config) return SEAT_CTRL_ERR_INVALID;

    // default values
    memset(config, 0, sizeof(seatctrl_config_t));
    config->can_device = "can0";
    config->debug_raw = false;
    config->debug_ctl = true;
    config->debug_stats = true;
    config->debug_verbose = false;
    config->motor_rpm = DEFAULT_RPM; // WARNING! uint8_t !!!
    config->command_timeout = DEFAULT_OPERATION_TIMEOUT;

    if (getenv("SC_CAN")) config->can_device = getenv("SC_CAN");

    if (getenv("SC_RAW")) config->debug_raw = atoi(getenv("SC_RAW"));
    if (getenv("SC_CTL")) config->debug_ctl = atoi(getenv("SC_CTL"));
    if (getenv("SC_STAT")) config->debug_stats = atoi(getenv("SC_STAT"));
    if (getenv("SC_VERBOSE")) config->debug_verbose = atoi(getenv("SC_VERBOSE"));

    if (getenv("SC_RPM")) config->motor_rpm = atoi(getenv("SC_RPM"));
    if (getenv("SC_TIMEOUT")) config->command_timeout = atoi(getenv("SC_TIMEOUT"));

    printf("### seatctrl_config: { can:%s, motor_rpm:%d, operation_timeout:%d }\n",
            config->can_device, config->motor_rpm, config->command_timeout);
    printf("### seatctrl_logs  : { raw:%d, ctl:%d, stat:%d, verb:%d }\n",
            config->debug_raw, config->debug_ctl, config->debug_stats, config->debug_verbose);
    // args check:
    if (config->motor_rpm < 1 || config->motor_rpm > 254) {
        printf("### SC_RPM: %d, range is [1..254]\n", config->motor_rpm);
        config->motor_rpm = DEFAULT_RPM;
        return SEAT_CTRL_ERR_INVALID;
    }
    return SEAT_CTRL_OK;
}


/**
 * @brief See seat_controller.h
 */
error_t seatctrl_init_ctx(seatctrl_context_t *ctx, seatctrl_config_t *config)
{
    if (!ctx || !config) {
        printf(SELF_INIT "ERR: context or config are NULL!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    if (!config->can_device) {
        printf(SELF_INIT "ERR: config.can_device is NULL!\n");
        return SEAT_CTRL_ERR_INVALID;
    }

    printf(SELF_INIT "### Initializing context from config: %s\n", config->can_device);
#if 0 // disabled because it relies on possible unitialized memory
    if (ctx->magic == SEAT_CTRL_CONTEXT_MAGIC) {
        printf(SELF_INIT "WARNING: Called on initialized context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
#endif
    memset(ctx, 0, sizeof(seatctrl_context_t));
    ctx->magic = SEAT_CTRL_CONTEXT_MAGIC;

    ctx->config = *config; // copy

    ctx->desired_position = MOTOR_POS_INVALID;
    ctx->desired_direction = MotorDirection::OFF;

    ctx->motor1_mov_state = MotorDirection::INV;
    ctx->motor1_learning_state = LearningState::Invalid;
    ctx->motor1_pos = MOTOR_POS_INVALID; // haven't been read yet, invalid(-1)=not learned(255)

    // invalidate for seatctrl_open()
    ctx->socket = SOCKET_INVALID;
    ctx->thread_id = (pthread_t)0;
    ctx->event_cb = NULL;
    ctx->event_cb_user_data = NULL;

    return SEAT_CTRL_OK;
}


/**
 * @brief See seat_controller.h
 */
error_t seatctrl_open(seatctrl_context_t *ctx)
{
    struct sockaddr_can addr;
    struct ifreq ifr;
    int rc = SEAT_CTRL_ERR;

    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC || !ctx->config.can_device) {
        printf(SELF_OPEN "ERR: Invalid Context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    printf(SELF_OPEN "### Opening: %s\n", ctx->config.can_device);
    if (ctx->socket != SOCKET_INVALID) {
        printf(SELF_INIT "ERR: Socket already initialized!\n");
        return SEAT_CTRL_ERR;
    }
    if (ctx->running || ctx->thread_id != (pthread_t)0) {
        printf(SELF_OPEN "ERR: Thread already initialized!\n");
        return SEAT_CTRL_ERR;
    }

    if ((ctx->socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
    {
        perror(SELF_OPEN "SocketCAN errror!");
        ctx->socket = SOCKET_INVALID;
        return SEAT_CTRL_ERR_NO_CAN;
    }
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ctx->config.can_device, IFNAMSIZ-1); // max 16!
    rc = ioctl(ctx->socket, SIOCGIFINDEX, &ifr);
    if (rc == -1) {
        perror("ioctl(SIOCGIFINDEX) failed");
        printf(SELF_OPEN "ERR: Could't find interrface index of %s\n", ctx->config.can_device);
        ifr.ifr_ifindex = -1;
        //return SEAT_CTRL_ERR_CAN_IO;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(ctx->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror(SELF_OPEN "Socket CAN bind error");
        return SEAT_CTRL_ERR_CAN_BIND;
    }

    // set 1 sec timeout
    timeval tv = { 1, 0 };

    rc = setsockopt(ctx->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (rc != 0) {
        perror(SELF_OPEN "setsockopt(SO_RCVTIMEO) error");
        // FIXME: invalidate ctx->thread_id to prevent joining on it
        // abort?
    }

    rc = pthread_create(&ctx->thread_id, NULL, seatctrl_threadFunc, (void *)ctx);
    if (rc != 0) {
        perror(SELF_OPEN "CAN handler thread error");
        // FIXME: invalidate ctx->thread_id to prevent joining on it
        ctx->thread_id = (pthread_t)0;
        return SEAT_CTRL_ERR;
    }

    printf(SELF_OPEN "### SocketCAN opened.\n");

    // FIXME: wait some time and check if SECU1_STAT signals are incoming from the thread
    return SEAT_CTRL_OK;
}


/**
 * @brief See seat_controller.h
 */
error_t seatctrl_set_event_callback(seatctrl_context_t *ctx, seatctrl_event_cb_t cb, void* user_data)
{
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf(SELF_SETPOS_CB "ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }

    ctx->event_cb = cb;
    ctx->event_cb_user_data = user_data;

    printf(SELF_SETPOS_CB "### Set cb:%p, data:%p\n", (void*)cb, user_data);
    return SEAT_CTRL_OK;
}

/**
 * @brief See seat_controller.h
 */
error_t seatctrl_close(seatctrl_context_t *ctx)
{
    int rc = SEAT_CTRL_OK;
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf(SELF_CLOSE "ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }

    printf(SELF_CLOSE "socket: %d, running:%d\n", ctx->socket, ctx->running);

    // try aborting reader thread
    ctx->running = false;
    usleep(5000u);

    if (ctx->socket != SOCKET_INVALID) {
        if (ctx->config.debug_verbose) printf(SELF_CLOSE "### closing SocketCAN...\n");
        int res = close(ctx->socket);
        if (res < 0) {
            perror(SELF_CLOSE "SocketCAN close");
            rc = SEAT_CTRL_ERR;
        }
        ctx->socket = SOCKET_INVALID;
    }

    if (ctx->thread_id) {
        if (pthread_self() == ctx->thread_id) {
            if (ctx->config.debug_verbose) {
                printf(SELF_CLOSE "### Skipped stopping from same thread: %p ...\n", (void *)ctx->thread_id);
            }
        } else {
            if (ctx->config.debug_verbose)
                printf(SELF_CLOSE "### Waiting for thread: %p ...\n", (void *)ctx->thread_id);
            int res = pthread_join(ctx->thread_id, NULL);
            if (res != 0) {
                errno = res;
                perror("pthread_join failed");
                rc = SEAT_CTRL_ERR;
            }
        }
        ctx->thread_id = (pthread_t)0;
    }
    // FIXME: if (ctx->can_device != NULL) { free(ctx->can_device); ctx->can_device = NULL }
    return rc;
}
