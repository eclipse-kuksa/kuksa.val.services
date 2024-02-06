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

#include <mutex>

// cantools generated code from .dbc
#include "CAN.h"

#include "seat_controller.h"


//// function dump prefix ////

#define PREFIX_CAN      "        [CAN]: "
#define PREFIX_CTL      "   [CTL Loop]: "
#define PREFIX_STAT     " [SECU2_STAT]: "
#define PREFIX_APP      "[SeatCtrl:"
#define SELF_INIT         PREFIX_APP ":init_ctx] "
#define SELF_OPEN         PREFIX_APP ":open] "
#define SELF_CLOSE        PREFIX_APP ":close] "
#define SELF_CMD1         PREFIX_APP ":_send_cmd] "
#define SELF_STOPMOV      PREFIX_APP ":stop_move] "
#define SELF_SETPOS       PREFIX_APP ":set_position] "
#define SELF_SETTILT       PREFIX_APP ":set_tilt] "
#define SELF_SETHEIGHT       PREFIX_APP ":set_height] "
#define SELF_SETPOS_CB    PREFIX_APP ":set_pos_cb] "

std::mutex ctx_mutex;

//////////////////////////
// private declarations //
//////////////////////////

int64_t get_ts();
const char* pos_mov_state_string(int dir);
const char* rec_pos_mov_state_string(int dir);
int pos_mov_state(const char* str);
const char* tilt_mov_state_string(int dir);
const char* rec_tilt_mov_state_string(int dir);
int tilt_mov_state(const char* str);
const char* height_mov_state_string(int dir);

void print_secu2_cmd_1(const char* prefix, CAN_secu2_cmd_1_t *cmd);
void print_secu2_stat(const char* prefix, CAN_secu2_stat_t *stat);
void print_secu1_cmd_1(const char* prefix, CAN_secu1_cmd_1_t *cmd);
void print_secu1_stat(const char* prefix, CAN_secu1_stat_t *stat);
void print_can_raw(const struct can_frame *frame, bool is_received);
void print_ctl_pos_stat(seatctrl_context_t *ctx, const char* prefix);
void print_ctl_tilt_stat(seatctrl_context_t *ctx, const char* prefix);
void print_ctl_height_stat(seatctrl_context_t *ctx, const char* prefix);

error_t handle_secu2_stat(seatctrl_context_t *ctx, const struct can_frame *frame);
error_t handle_secu1_stat(seatctrl_context_t *ctx, const struct can_frame *frame);
error_t seatctrl_send_ecu2_cmd1(seatctrl_context_t *ctx, uint8_t motor1_dir, uint8_t motor1_rpm, uint8_t motor);
error_t seatctrl_send_ecu1_cmd1(seatctrl_context_t *ctx, uint8_t motor1_dir, uint8_t motor1_rpm, uint8_t motor);

error_t seatctrl_control_ecu12_loop(seatctrl_context_t *ctx);


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
 * @brief Returns string description for MotorPosDirection enum values.
 *
 * @param dir
 * @return const char*
 */
const char* pos_mov_state_string(int dir)
{
    switch (dir) {
        case MotorPosDirection::POS_OFF: return "OFF";
        case MotorPosDirection::POS_INC: return "INC";
        case MotorPosDirection::POS_DEC: return "DEC";
        case MotorPosDirection::POS_INV: return "INV";
        default: return "Undefined!";
    }
}

/**
 * @brief Returns string description for MotorPosDirection enum values.
 *
 * @param dir
 * @return const char*
 */
const char* rec_pos_mov_state_string(int dir)
{
    switch (dir) {
        case RecMotorPosDirection::REC_POS_OFF: return "OFF";
        case RecMotorPosDirection::REC_POS_INC: return "INC";
        case RecMotorPosDirection::REC_POS_DEC: return "DEC";
        case RecMotorPosDirection::REC_POS_INV: return "INV";
        default: return "Undefined!";
    }
}

/**
 * @brief Returns int for strings according to MotorPosDirection enum values. Used to convert between sending direction and receiving direction.
 *
 * @param str
 * @return const int
 */
int pos_mov_state(const char* str)
{
    if (strcmp(str, "OFF") == 0){
        return MotorPosDirection::POS_OFF;
    }
    else if(strcmp(str, "INC") == 0){
        return MotorPosDirection::POS_INC;
    }
    else if (strcmp(str, "DEC") == 0){
        return MotorPosDirection::POS_DEC;
    }
    else if(strcmp(str, "INV") == 0){
        return MotorPosDirection::POS_INV;
    }
    else {
        return -2;
    }
}

/**
 * @brief Returns string description for MotorTiltDirection enum values.
 *
 * @param dir
 * @return const char*
 */
const char* tilt_mov_state_string(int dir)
{
    switch (dir) {
        case MotorTiltDirection::TILT_OFF: return "OFF";
        case MotorTiltDirection::TILT_INC: return "INC";
        case MotorTiltDirection::TILT_DEC: return "DEC";
        case MotorTiltDirection::TILT_INV: return "INV";
        default: return "Undefined!";
    }
}

/**
 * @brief Returns string description for MotorPosDirection enum values.
 *
 * @param dir
 * @return const char*
 */
const char* rec_tilt_mov_state_string(int dir)
{
    switch (dir) {
        case RecMotorTiltDirection::REC_TILT_OFF: return "OFF";
        case RecMotorTiltDirection::REC_TILT_INC: return "INC";
        case RecMotorTiltDirection::REC_TILT_DEC: return "DEC";
        case RecMotorTiltDirection::REC_TILT_INV: return "INV";
        default: return "Undefined!";
    }
}

/**
 * @brief Returns int for strings according to MotorTiltDirection enum values. Used to convert between sending direction and receiving direction.
 *
 * @param str
 * @return const int
 */
int tilt_mov_state(const char* str)
{
    if (strcmp(str, "OFF") == 0){
        return MotorTiltDirection::TILT_OFF;
    }
    else if(strcmp(str, "INC") == 0){
        return MotorTiltDirection::TILT_INC;
    }
    else if (strcmp(str, "DEC") == 0){
        return MotorTiltDirection::TILT_DEC;
    }
    else if(strcmp(str, "INV") == 0){
        return MotorTiltDirection::TILT_INV;
    }
    else {
        return -2;
    }
}

/**
 * @brief Returns string description for MotorHeightDirection enum values.
 *
 * @param dir
 * @return const char*
 */
const char* height_mov_state_string(int dir)
{
    switch (dir) {
        case MotorHeightDirection::HEIGHT_OFF: return "OFF";
        case MotorHeightDirection::HEIGHT_INC: return "INC";
        case MotorHeightDirection::HEIGHT_DEC: return "DEC";
        case MotorHeightDirection::HEIGHT_INV: return "INV";
        default: return "Undefined!";
    }
}


/**
 * @brief Describes CAN_secu2_stat_t.motorX_learning_state
 *
 * @param state
 * @return const char*
 */
const char* pos_learning_state_string(int state)
{
    switch (state) {
        case PosLearningState::PosNotLearned:
            return "NOK";
        case PosLearningState::PosLearned:
            return "OK";
        case PosLearningState::PosInvalid:
            return "INV";
        default:
            return "Undefined!";
    }
}

/**
 * @brief Describes CAN_secu2_stat_t.motorX_learning_state
 *
 * @param state
 * @return const char*
 */
const char* tilt_learning_state_string(int state)
{
    switch (state) {
        case TiltLearningState::TiltNotLearned:
            return "NOK";
        case TiltLearningState::TiltLearned:
            return "OK";
        case TiltLearningState::TiltInvalid:
            return "INV";
        default:
            return "Undefined!";
    }
}

/**
 * @brief Describes CAN_secu2_stat_t.motorX_learning_state
 *
 * @param state
 * @return const char*
 */
const char* height_learning_state_string(int state)
{
    switch (state) {
        case HeightLearningState::HeightNotLearned:
            return "NOK";
        case HeightLearningState::HeightLearned:
            return "OK";
        case HeightLearningState::HeightInvalid:
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
void print_ctl_pos_stat(seatctrl_context_t *ctx, const char* prefix)
{
    int64_t elapsed = ctx->command_pos_ts != 0 ? get_ts() - ctx->command_pos_ts : -1;
    printf("%sPosition:{ pos:%3d%%, %-3s } --> target:{ pos:%3d%%, %3s }, elapsed: %" PRId64 " ms.\n",
            prefix,
            ctx->motor_pos,
            pos_mov_state_string(ctx->motor_pos_mov_state),
            ctx->desired_position,
            pos_mov_state_string(ctx->desired_pos_direction),
            elapsed
        );
}


/**
 * @brief Prints CTL stats (active command, desired position, etc) on stdout
 *
 * @param ctx SeatCtrl context
 * @param prefix string to print before stats
 */
void print_ctl_tilt_stat(seatctrl_context_t *ctx, const char* prefix)
{
    int64_t elapsed = ctx->command_tilt_ts != 0 ? get_ts() - ctx->command_tilt_ts : -1;
    printf("%sTilt:{ pos:%3d%%, %-3s } --> target:{ pos:%3d%%, %3s }, elapsed: %" PRId64 " ms.\n",
            prefix,
            ctx->motor_tilt,
            tilt_mov_state_string(ctx->motor_tilt_mov_state),
            ctx->desired_tilt,
            tilt_mov_state_string(ctx->desired_tilt_direction),
            elapsed
        );
}


/**
 * @brief Prints CTL stats (active command, desired position, etc) on stdout
 *
 * @param ctx SeatCtrl context
 * @param prefix string to print before stats
 */
void print_ctl_height_stat(seatctrl_context_t *ctx, const char* prefix)
{
    int64_t elapsed = ctx->command_height_ts != 0 ? get_ts() - ctx->command_height_ts : -1;
    printf("%sHeight:{ pos:%3d%%, %-3s } --> target:{ pos:%3d%%, %3s }, elapsed: %" PRId64 " ms.\n",
            prefix,
            ctx->motor_height,
            height_mov_state_string(ctx->motor_height_mov_state),
            ctx->desired_height,
            height_mov_state_string(ctx->desired_height_direction),
            elapsed
        );
}



/**
 * @brief Prints CAN_secu2_cmd_1_t* in human readable format to stdout
 *
 * @param prefix string to print before stats
 * @param cmd CAN_secu2_cmd_1_t*
 */
void print_secu1_cmd_1(const char* prefix, CAN_secu1_cmd_1_t *cmd)
{
#ifdef SEAT_CTRL_ALL_MOTORS // reduce extra dumps on console, we care about motor1 only
    printf("%s[SECU1]{ m1_cmd: %s, m1_rpm: %d, m2_cmd: %s, m2_rpm: %d, m3_cmd: %s, m3_rpm: %d, m4_cmd: %s, m4_rpm: %d }\n",
            prefix,
            height_mov_state_string(cmd->motor1_manual_cmd), cmd->motor1_set_rpm * 100,
            height_mov_state_string(cmd->motor2_manual_cmd), cmd->motor2_set_rpm * 100,
            height_mov_state_string(cmd->motor3_manual_cmd), cmd->motor3_set_rpm * 100,
            height_mov_state_string(cmd->motor4_manual_cmd), cmd->motor4_set_rpm * 100);
#else
    printf("%s[SECU1]{ motor1_cmd: %s, motor1_rpm: %d }\n",
            prefix,
            height_mov_state_string(cmd->motor1_manual_cmd), cmd->motor1_set_rpm * 100);
#endif
}

/**
 * @brief Prints CAN_secu2_cmd_1_t* in human readable format to stdout
 *
 * @param prefix string to print before stats
 * @param cmd CAN_secu2_cmd_1_t*
 */
void print_secu2_cmd_1(const char* prefix, CAN_secu2_cmd_1_t *cmd)
{
    #ifdef SEAT_CTRL_ALL_MOTORS // reduce extra dumps on console, we care about motor1 only
    printf("%s[SECU2]{ m1_cmd: %s, m1_rpm: %d, m2_cmd: %s, m2_rpm: %d, m3_cmd: %s, m3_rpm: %d, m4_cmd: %s, m4_rpm: %d }\n",
            prefix,
            pos_mov_state_string(cmd->motor1_manual_cmd), cmd->motor1_set_rpm * 100,
            pos_mov_state_string(cmd->motor2_manual_cmd), cmd->motor2_set_rpm * 100,
            tilt_mov_state_string(cmd->motor3_manual_cmd), cmd->motor3_set_rpm * 100,
            tilt_mov_state_string(cmd->motor4_manual_cmd), cmd->motor4_set_rpm * 100);
#else
    printf("%s[SECU2]{ motor1_cmd: %s, motor1_rpm: %d, motor3_cmd: %s, motor3_rpm: %d, }\n",
            prefix,
            pos_mov_state_string(cmd->motor1_manual_cmd), cmd->motor1_set_rpm * 100,
            tilt_mov_state_string(cmd->motor3_manual_cmd), cmd->motor3_set_rpm * 100);
#endif
   
}


/**
 * @brief Prints CAN_secu2_stat_t* in human readable format to stdout
 *
 * @param prefix
 * @param stat
 */
void print_secu2_stat(const char* prefix, CAN_secu2_stat_t *stat)
{
    printf("%s{ motor1_pos:%3d%%, motor1_mov_state: %-3s, motor1_learning_state: %s; motor3_pos:%3d%%, motor3_mov_state: %-3s, motor3_learning_state: %s }\n",
            prefix,
            stat->motor1_pos, // CAN_secu2_stat_motor1_pos_decode() - not generated if float code is disabled! Make sure scaling remains "default"!
            pos_mov_state_string(stat->motor1_mov_state),
            pos_learning_state_string(stat->motor1_learning_state),
            stat->motor3_pos, // CAN_secu2_stat_motor3_pos_decode() - not generated if float code is disabled! Make sure scaling remains "default"!
            height_mov_state_string(stat->motor3_mov_state),
            tilt_learning_state_string(stat->motor3_learning_state)
    );
}

/**
 * @brief Prints CAN_secu1_stat_t* in human readable format to stdout
 *
 * @param prefix
 * @param stat
 */
void print_secu1_stat(const char* prefix, CAN_secu1_stat_t *stat)
{
    printf("%s{ motor1_pos:%3d%%, motor1_mov_state: %-3s, motor1_learning_state: %s }\n",
            prefix,
            stat->motor1_pos, // CAN_secu2_stat_motor1_pos_decode() - not generated if float code is disabled! Make sure scaling remains "default"!
            height_mov_state_string(stat->motor1_mov_state),
            height_learning_state_string(stat->motor1_learning_state)
    );
}


/**
 * @brief Handler function for processing SECUx_STAT commands
 *
 * @param ctx SeatCtrl context
 * @param frame can_frame with CanID = CAN_SECU2_STAT_FRAME_ID
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t handle_secu2_stat(seatctrl_context_t *ctx, const struct can_frame *frame)
{
    CAN_secu2_stat_t stat;
    memset(&stat, 0, sizeof(CAN_secu2_stat_t));

    error_t ret = SEAT_CTRL_ERR_INVALID;

    if (frame->can_id != CAN_SECU2_STAT_FRAME_ID) {
        printf(PREFIX_CTL "ERR: Not a CAN_SECU2_STAT_FRAME_ID frame! (%d)\n", frame->can_id);
        return SEAT_CTRL_ERR_INVALID;
    }
    int rc = CAN_secu2_stat_unpack(&stat, frame->data, frame->can_dlc);
    if (rc != 0) {
        printf(PREFIX_CTL "ERR: Failed unpacking CAN_SECU2_STAT_FRAME_ID frame!\n");
        return SEAT_CTRL_ERR;
    }

    // if values in range -> update motor1 last known pos. helpful against cangen attacks
    if (CAN_secu2_stat_motor1_pos_is_in_range(stat.motor1_pos) &&
        ((int)stat.motor1_pos <= 100 || (int)stat.motor1_pos == MOTOR_POS_INVALID) && // range always positive
        CAN_secu2_stat_motor1_mov_state_is_in_range(stat.motor1_mov_state) &&
        CAN_secu2_stat_motor1_learning_state_is_in_range(stat.motor1_learning_state))
    {
        if (ctx->config.debug_stats) {
            // dump unique?
            if (ctx->config.debug_verbose ||
                ctx->motor_pos != stat.motor1_pos ||
                ctx->motor_pos_learning_state != stat.motor1_learning_state ||
                ctx->motor_pos_mov_state != stat.motor1_mov_state)
            {
                print_secu2_stat(PREFIX_STAT, &stat);
            }
        }

        if (ctx->running && ctx->event_cb != NULL && ctx->motor_pos != stat.motor1_pos) {
            if (ctx->config.debug_verbose) printf(PREFIX_CTL " calling cb: %p(Motor1Pos, %d)\n", (void*)ctx->event_cb, stat.motor1_pos);
            ctx->event_cb(SeatCtrlEvent::MotorPos, stat.motor1_pos, ctx->event_cb_user_data);
        }

        int mov_state = pos_mov_state(rec_pos_mov_state_string(stat.motor1_mov_state));
        ctx->motor_pos_mov_state = mov_state;
        ctx->motor_pos_learning_state = stat.motor1_learning_state;
        ctx->motor_pos = stat.motor1_pos; // decode?

        ret = SEAT_CTRL_OK;
    }

    // if values in range -> update motor2 last known pos. helpful against cangen attacks
    if (
        CAN_secu2_stat_motor3_pos_is_in_range(stat.motor3_pos) &&
        ((int)stat.motor3_pos <= 100 || (int)stat.motor3_pos == MOTOR_POS_INVALID) && // range always positive
        CAN_secu2_stat_motor3_mov_state_is_in_range(stat.motor3_mov_state) &&
        CAN_secu2_stat_motor3_learning_state_is_in_range(stat.motor3_learning_state))
    {
        if (ctx->config.debug_stats) {
            // dump unique?
            if (ctx->config.debug_verbose ||
                ctx->motor_tilt != stat.motor3_pos ||
                ctx->motor_tilt_learning_state != stat.motor3_learning_state ||
                ctx->motor_tilt_mov_state != stat.motor3_mov_state)
            {
                print_secu2_stat(PREFIX_STAT, &stat);
            }
        }
        if (ctx->running && ctx->event_cb != NULL && ctx->motor_tilt != stat.motor3_pos) {
            if (ctx->config.debug_verbose) printf(PREFIX_CTL " calling cb: %p(Motor2Pos, %d)\n", (void*)ctx->event_cb, stat.motor3_pos);
            ctx->event_cb(SeatCtrlEvent::MotorTilt, stat.motor3_pos, ctx->event_cb_user_data);
        }

        int mov_state = tilt_mov_state(rec_tilt_mov_state_string(stat.motor3_mov_state));
        ctx->motor_tilt_mov_state = mov_state;
        ctx->motor_tilt_learning_state = stat.motor3_learning_state;
        ctx->motor_tilt = stat.motor3_pos; // decode?
        ret = SEAT_CTRL_OK;
    }

    return ret;
}

/**
 * @brief Handler function for processing SECUx_STAT commands
 *
 * @param ctx SeatCtrl context
 * @param frame can_frame with CanID = CAN_SECU1_STAT_FRAME_ID
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t handle_secu1_stat(seatctrl_context_t *ctx, const struct can_frame *frame)
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

    // if values in range -> update motor3 last known pos. helpful against cangen attacks
    if (CAN_secu1_stat_motor1_pos_is_in_range(stat.motor1_pos) &&
        ((int)stat.motor1_pos <= 100 || (int)stat.motor1_pos == MOTOR_POS_INVALID) && // range always positive
        CAN_secu1_stat_motor1_mov_state_is_in_range(stat.motor1_mov_state) &&
        CAN_secu1_stat_motor1_learning_state_is_in_range(stat.motor1_learning_state) &&
        CAN_secu1_stat_motor1_pos_is_in_range(stat.motor3_pos))
    {
        if (ctx->config.debug_stats) {
            // dump unique?
            if (ctx->config.debug_verbose ||
                ctx->motor_height != stat.motor1_pos ||
                ctx->motor_height_learning_state != stat.motor1_learning_state ||
                ctx->motor_height_mov_state != stat.motor1_mov_state)
            {
                print_secu1_stat(PREFIX_STAT, &stat);
            }
        }

        if (ctx->running && ctx->event_cb != NULL && ctx->motor_height != stat.motor1_pos) {
            if (ctx->config.debug_verbose) printf(PREFIX_CTL " calling cb: %p(Motor3Pos, %d)\n", (void*)ctx->event_cb, stat.motor1_pos);
            ctx->event_cb(SeatCtrlEvent::MotorHeight, stat.motor1_pos, ctx->event_cb_user_data);
        }

        ctx->motor_height_mov_state = stat.motor1_mov_state;
        ctx->motor_height_learning_state = stat.motor1_learning_state;
        ctx->motor_height = stat.motor1_pos; // decode?

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
        (( ctx->command_pos_ts > 0 && ctx->desired_pos_direction != MotorPosDirection::POS_OFF &&
        ctx->desired_position != MOTOR_POS_INVALID) ||
        ( ctx->command_tilt_ts > 0 &&ctx->desired_tilt_direction != MotorTiltDirection::TILT_OFF &&
        ctx->desired_tilt != MOTOR_POS_INVALID) ||
        (ctx->command_height_ts > 0 && ctx->desired_height_direction != MotorHeightDirection::HEIGHT_OFF &&
        ctx->desired_height != MOTOR_POS_INVALID))) {
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
    return (int)ctx->motor_pos; // Last position or MOTOR_POS_INVALID
}

/**
 * @brief See seat_controller.h
 */
int seatctrl_get_tilt(seatctrl_context_t *ctx)
{
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf("[seatctrl_get_tilt] ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    if (!ctx->running) {
        return SEAT_CTRL_ERR; // CTR not yet started or stopping
    }
    return (int)ctx->motor_tilt; // Last position or MOTOR_POS_INVALID
}

/**
 * @brief See seat_controller.h
 */
int seatctrl_get_height(seatctrl_context_t *ctx)
{
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf("[seatctrl_get_height] ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    if (!ctx->running) {
        return SEAT_CTRL_ERR; // CTR not yet started or stopping
    }
    return (int)ctx->motor_height; // Last position or MOTOR_POS_INVALID
}

// TODO: move in context
static int last_ctl_pos = MOTOR_POS_INVALID;
static int last_ctl_pos_dir = 0;
static int last_ctl_tilt = MOTOR_POS_INVALID;
static int last_ctl_tilt_dir = 0;
static int last_ctl_height = MOTOR_POS_INVALID;
static int last_ctl_height_dir = 0;

static bool learned_mode = true;         // assume motor learned mode
static int64_t learned_mode_changed = 0; // rate limit state change dumps
#define LEARNED_MODE_RATE	10*1000L     // timeout (ms) to ignore dumps about learned state change

/**
 * @brief Handles Seat Adjustment Control Loop (CTL)
 *
 * @param ctx SeatCtrl context
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error
 */
error_t seatctrl_control_ecu12_loop(seatctrl_context_t *ctx)
{
    error_t rc = SEAT_CTRL_OK;
    // FIXME: Handle ctx->motor_pos_learning_state == LearningState::NotLearned
    if (ctx->motor_pos_learning_state == PosLearningState::PosNotLearned || ctx->motor_tilt_learning_state == TiltLearningState::TiltNotLearned || ctx->motor_height_learning_state == HeightLearningState::HeightNotLearned) {
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
    if (!learned_mode && ctx->motor_pos_learning_state == PosLearningState::PosLearned && ctx->motor_tilt_learning_state == TiltLearningState::TiltLearned && ctx->motor_height_learning_state == HeightLearningState::HeightLearned) {
        int ts = get_ts();
        if (ts - learned_mode_changed > LEARNED_MODE_RATE) {
            printf("\n");
            printf(PREFIX_CTL "*** ECU changed to: learned state!\n");
            fflush(stdout);
            learned_mode_changed = ts;
            learned_mode = true;
        }
    }

    if(learned_mode){
        //   In that state normalization loop must be done on real hw.
        if (is_ctl_running(ctx)) {
            int64_t elapsed_motor1 = 0;
            int64_t elapsed_motor2 = 0;
            int64_t elapsed_motor3 = 0;

            if(ctx->pos_running){
                // if all commands come in at the same time add elapsed times so it does not timeout while waiting for other motors (worst case timeout 3x config.command_timeout)
                elapsed_motor1 = get_ts() - ctx->command_pos_ts + elapsed_motor2 + elapsed_motor3;
                // Preliminary phase: operation was just scheduled (up to 500ms ago),
                // but can signal may not yet come, i.e. waiting for motor tor start moving
                if (elapsed_motor1 < 500 && ctx->motor_pos_mov_state == MotorPosDirection::POS_OFF && ctx->motor_pos != ctx->desired_position) {
                    printf(PREFIX_CTL "* Seat Adjustment[Position] to (%d, %s) active, waiting motor movement for %" PRId64 "ms.\n",
                            ctx->desired_position,
                            pos_mov_state_string(ctx->desired_pos_direction),
                            elapsed_motor1);
                    ::usleep(1000);
                    return SEAT_CTRL_OK;
                }

                // reduce frequency of dumps, only if something relevant changed,
                // but don't cache states when command was just started (e.g. motor off warning will be dumped always)
                if (last_ctl_pos != ctx->motor_pos || last_ctl_pos_dir != ctx->motor_pos_mov_state) {
                    if (ctx->config.debug_ctl) print_ctl_pos_stat(ctx, PREFIX_CTL);
                    if (ctx->motor_pos_mov_state != ctx->desired_pos_direction && ctx->motor_pos != ctx->desired_position) {
                        printf("\n");
                        printf(PREFIX_CTL "WARN: *** Seat Adjustment[Position] to (%d, %s) active, but motor1_mov_state is %s.\n",
                                ctx->desired_position,
                                pos_mov_state_string(ctx->desired_pos_direction),
                                pos_mov_state_string(ctx->motor_pos_mov_state));
                        // Workaround for possible "bug" in seat adjuster ECU that is stopping (OFF) at
                        // some thresholds at both ends of the range (e.g. 14% and 80%)
                        if (ctx->motor_pos_mov_state == MotorPosDirection::POS_OFF) {
                            // printf(PREFIX_CTL " >>> Sending MotorPosOff command...\n");
                            // error_t rc0 = seatctrl_send_ecu2_cmd1(ctx, MotorPosDirection::POS_OFF, 0, 1); // off, 0rpm
                            // if (rc0 != SEAT_CTRL_OK) {
                            //     perror(PREFIX_CTL "seatctrl_send_ecu2_cmd1(OFF) error");
                            // }
                            ::usleep(100*1000L); // it needs some time to process the off command. TODO: check with ECU team
                            printf(PREFIX_CTL ">>> Re-sending: SECU2_CMD_1 [ motor1_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                                    ctx->motor_pos, ctx->desired_position, pos_mov_state_string(ctx->desired_pos_direction), ctx->command_pos_ts);
                            rc = seatctrl_send_ecu2_cmd1(ctx, ctx->desired_pos_direction, ctx->config.motor_pos_rpm, 1);
                            if (rc != SEAT_CTRL_OK) {
                                perror(PREFIX_CTL "seatctrl_send_ecu2_cmd1(desired_pos) error");
                            }
                        }
                        printf("\n");
                    }
                    if (ctx->motor_pos == MOTOR_POS_INVALID) {
                        printf(PREFIX_CTL "WARN: *** Seat Adjustment[Position] to (%d, %s) active, but motor1_pos is: %d.\n",
                                ctx->desired_position,
                                pos_mov_state_string(ctx->desired_pos_direction),
                                ctx->motor_pos);
                                // break; ?
                    }
                    last_ctl_pos_dir = ctx->motor_pos_mov_state;
                    last_ctl_pos = ctx->motor_pos;
                }
                if ( ctx->motor_pos != MOTOR_POS_INVALID &&
                    ((ctx->desired_pos_direction == MotorPosDirection::POS_INC && ctx->motor_pos >= ctx->desired_position) ||
                    (ctx->desired_pos_direction == MotorPosDirection::POS_DEC && ctx->motor_pos <= ctx->desired_position) ))
                {
                    // Terminal state, reached destination
                    printf(PREFIX_CTL "*** Seat Adjustment[Position] (%d, %s) finished at pos: %d for %" PRId64 "ms.\n",
                            ctx->desired_position,
                            pos_mov_state_string(ctx->desired_pos_direction),
                            ctx->motor_pos,
                            elapsed_motor1);
                    seatctrl_stop_pos_movement(ctx);
                    // invalidate last states
                    last_ctl_pos_dir = -1;
                    last_ctl_pos = MOTOR_POS_INVALID;
                } else
                if (elapsed_motor1 > ctx->config.command_timeout) {
                    // stop movement due to timeout
                    printf(PREFIX_CTL "WARN: *** Seat adjustment[Position] to (%d, %s) timed out (%" PRId64 "ms). Stopping motors.\n",
                            ctx->desired_position,
                            pos_mov_state_string(ctx->desired_pos_direction),
                            elapsed_motor1);
                    seatctrl_stop_pos_movement(ctx);
                    // invalidate last states
                    last_ctl_pos_dir = -1;
                    last_ctl_pos = MOTOR_POS_INVALID;
                }
            }

            if(ctx->tilt_running){
                elapsed_motor2 = get_ts() - ctx->command_tilt_ts + elapsed_motor1 + elapsed_motor3;
                // Preliminary phase: operation was just scheduled (up to 500ms ago),
                // but can signal may not yet come, i.e. waiting for motor tor start moving
                if (elapsed_motor2 < 500 && ctx->motor_tilt_mov_state == MotorTiltDirection::TILT_OFF && ctx->motor_tilt != ctx->desired_tilt) {
                    printf(PREFIX_CTL "* Seat Adjustment[Tilt] to (%d, %s) active, waiting motor movement for %" PRId64 "ms.\n",
                            ctx->desired_tilt,
                            tilt_mov_state_string(ctx->desired_tilt_direction),
                            elapsed_motor2);
                    ::usleep(1000);
                    return SEAT_CTRL_OK;
                }

                // reduce frequency of dumps, only if something relevant changed,
                // but don't cache states when command was just started (e.g. motor off warning will be dumped always)
                if (last_ctl_tilt != ctx->motor_tilt || last_ctl_tilt_dir != ctx->motor_tilt_mov_state) {
                    if (ctx->config.debug_ctl) print_ctl_tilt_stat(ctx, PREFIX_CTL);
                    if (ctx->motor_tilt_mov_state != ctx->desired_tilt_direction && ctx->motor_tilt != ctx->desired_tilt) {
                        printf("\n");
                        printf(PREFIX_CTL "WARN: *** Seat Adjustment[Tilt] to (%d, %s) active, but motor2_mov_state is %s.\n",
                                ctx->desired_tilt,
                                tilt_mov_state_string(ctx->desired_tilt_direction),
                                tilt_mov_state_string(ctx->motor_tilt_mov_state));
                        // Workaround for possible "bug" in seat adjuster ECU that is stopping (OFF) at
                        // some thresholds at both ends of the range (e.g. 14% and 80%)
                        if (ctx->motor_tilt_mov_state == MotorTiltDirection::TILT_OFF) {
                            printf(PREFIX_CTL " >>> Sending MotorTiltOff command...\n");
                            // error_t rc0 = seatctrl_send_ecu2_cmd1(ctx, MotorTiltDirection::TILT_OFF, 0, 3); // off, 0rpm
                            // if (rc0 != SEAT_CTRL_OK) {
                            //     perror(PREFIX_CTL "seatctrl_send_ecu2_cmd1(OFF) error");
                            // }
                            ::usleep(100*1000L); // it needs some time to process the off command. TODO: check with ECU team
                            printf(PREFIX_CTL ">>> Re-sending: SECU2_CMD_1 [ motor2_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                                    ctx->motor_tilt, ctx->desired_tilt, tilt_mov_state_string(ctx->desired_tilt_direction), ctx->command_tilt_ts);
                            rc = seatctrl_send_ecu2_cmd1(ctx, ctx->desired_tilt_direction, ctx->config.motor_tilt_rpm, 3);
                            if (rc != SEAT_CTRL_OK) {
                                perror(PREFIX_CTL "seatctrl_send_ecu2_cmd1(desired_pos) error");
                            }
                        }
                        printf("\n");
                    }
                    if (ctx->motor_tilt == MOTOR_POS_INVALID) {
                        printf(PREFIX_CTL "WARN: *** Seat Adjustment[Tilt] to (%d, %s) active, but motor2_pos is: %d.\n",
                                ctx->desired_tilt,
                                tilt_mov_state_string(ctx->desired_tilt_direction),
                                ctx->motor_tilt);
                                // break; ?
                    }
                    last_ctl_tilt_dir = ctx->motor_tilt_mov_state;
                    last_ctl_tilt = ctx->motor_tilt;
                }
                // FIXME: if desired_tilt_direction INC && ctx->desired_tilt >= ctx->motor_tilt
                if ( ctx->motor_tilt != MOTOR_POS_INVALID &&
                    ((ctx->desired_tilt_direction == MotorTiltDirection::TILT_INC && ctx->motor_tilt >= ctx->desired_tilt) ||
                    (ctx->desired_tilt_direction == MotorTiltDirection::TILT_DEC && ctx->motor_tilt <= ctx->desired_tilt) ))
                {
                    // Terminal state, reached destination
                    printf(PREFIX_CTL "*** Seat Adjustment[Tilt] (%d, %s) finished at pos: %d for %" PRId64 "ms.\n",
                            ctx->desired_tilt,
                            tilt_mov_state_string(ctx->desired_tilt_direction),
                            ctx->motor_tilt,
                            elapsed_motor2);
                    seatctrl_stop_tilt_movement(ctx);
                    // invalidate last states
                    last_ctl_tilt_dir = 0;
                    last_ctl_tilt = MOTOR_POS_INVALID;
                } else
                if (elapsed_motor2 > ctx->config.command_timeout) {
                    // stop movement due to timeout
                    printf(PREFIX_CTL "WARN: *** Seat adjustment[Tilt] to (%d, %s) timed out (%" PRId64 "ms). Stopping motors.\n",
                            ctx->desired_tilt,
                            tilt_mov_state_string(ctx->desired_tilt_direction),
                            elapsed_motor2);
                    seatctrl_stop_tilt_movement(ctx);
                    // invalidate last states
                    last_ctl_tilt_dir = 0;
                    last_ctl_tilt = MOTOR_POS_INVALID;
                }
            }

            if(ctx->height_running){
                elapsed_motor3 = get_ts() - ctx->command_height_ts + elapsed_motor2 + elapsed_motor1;
                // Preliminary phase: operation was just scheduled (up to 500ms ago),
                // but can signal may not yet come, i.e. waiting for motor tor start moving
                if (elapsed_motor3 < 500 && ctx->motor_height_mov_state == MotorHeightDirection::HEIGHT_OFF && ctx->motor_height != ctx->desired_height) {
                    printf(PREFIX_CTL "* Seat Adjustment[Height] to (%d, %s) active, waiting motor movement for %" PRId64 "ms.\n",
                            ctx->desired_height,
                            height_mov_state_string(ctx->desired_height_direction),
                            elapsed_motor3);
                    ::usleep(1000);
                    return SEAT_CTRL_OK;
                }

                // reduce frequency of dumps, only if something relevant changed,
                // but don't cache states when command was just started (e.g. motor off warning will be dumped always)
                if (last_ctl_height != ctx->motor_height || last_ctl_height_dir != ctx->motor_height_mov_state) {
                    if (ctx->config.debug_ctl) print_ctl_height_stat(ctx, PREFIX_CTL);
                    if (ctx->motor_height_mov_state != ctx->desired_height_direction && ctx->motor_height != ctx->desired_height) {
                        printf("\n");
                        printf(PREFIX_CTL "WARN: *** Seat Adjustment[Height] to (%d, %s) active, but motor3_mov_state is %s.\n",
                                ctx->desired_height,
                                height_mov_state_string(ctx->desired_height_direction),
                                height_mov_state_string(ctx->motor_height_mov_state));
                        // Workaround for possible "bug" in seat adjuster ECU that is stopping (OFF) at
                        // some thresholds at both ends of the range (e.g. 14% and 80%)
                        if (ctx->motor_height_mov_state == MotorHeightDirection::HEIGHT_OFF) {
                            printf(PREFIX_CTL " >>> Sending MotorHeightOff command...\n");
                            error_t rc0 = seatctrl_send_ecu1_cmd1(ctx, MotorHeightDirection::HEIGHT_OFF, 0, 1); // off, 0rpm
                            if (rc0 != SEAT_CTRL_OK) {
                                perror(PREFIX_CTL "seatctrl_send_ecu1_cmd1(OFF) error");
                            }
                            ::usleep(100*1000L); // it needs some time to process the off command. TODO: check with ECU team
                            printf(PREFIX_CTL ">>> Re-sending: SECU1_CMD_1 [ motor3_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                                    ctx->motor_height, ctx->desired_height, height_mov_state_string(ctx->desired_height_direction), ctx->command_height_ts);
                            error_t rc = seatctrl_send_ecu1_cmd1(ctx, ctx->desired_height_direction, ctx->config.motor_height_rpm, 1);
                            if (rc != SEAT_CTRL_OK) {
                                perror(PREFIX_CTL "seatctrl_send_ecu1_cmd1(desired_pos) error");
                            }
                        }
                        printf("\n");
                    }
                    if (ctx->motor_height == MOTOR_POS_INVALID) {
                        printf(PREFIX_CTL "WARN: *** Seat Adjustment[Height] to (%d, %s) active, but motor3_pos is: %d.\n",
                                ctx->desired_height,
                                height_mov_state_string(ctx->desired_height_direction),
                                ctx->motor_height);
                                // break; ?
                    }
                    last_ctl_height_dir = ctx->motor_height_mov_state;
                    last_ctl_height = ctx->motor_height;
                }
                // FIXME: if desired_height_direction INC && ctx->desired_height >= ctx->motor_height
                if ( ctx->motor_height != MOTOR_POS_INVALID &&
                    ((ctx->desired_height_direction == MotorHeightDirection::HEIGHT_INC && ctx->motor_height >= ctx->desired_height) ||
                    (ctx->desired_height_direction == MotorHeightDirection::HEIGHT_DEC && ctx->motor_height <= ctx->desired_height) ))
                {
                    // Terminal state, reached destination
                    printf(PREFIX_CTL "*** Seat Adjustment[Height] (%d, %s) finished at pos: %d for %" PRId64 "ms.\n",
                            ctx->desired_height,
                            height_mov_state_string(ctx->desired_height_direction),
                            ctx->motor_height,
                            elapsed_motor3);
                    seatctrl_stop_height_movement(ctx);
                    // invalidate last states
                    last_ctl_height_dir = 0;
                    last_ctl_height = MOTOR_POS_INVALID;
                } else
                if (elapsed_motor3 > ctx->config.command_timeout) {
                    // stop movement due to timeout
                    printf(PREFIX_CTL "WARN: *** Seat adjustment[Height] to (%d, %s) timed out (%" PRId64 "ms). Stopping motors.\n",
                            ctx->desired_height,
                            height_mov_state_string(ctx->desired_height_direction),
                            elapsed_motor3);
                    seatctrl_stop_height_movement(ctx);
                    // invalidate last states
                    last_ctl_height_dir = 0;
                    last_ctl_height = MOTOR_POS_INVALID;
                }
            }
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
    // BUGFIX always send 0, OFF to everything at the beginning
    bool frame1 = false;
    bool frame2 = false;
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
        if (frame.can_id == CAN_SECU2_STAT_FRAME_ID)
        {
            frame1 = true;
            if (handle_secu2_stat(ctx, &frame) != SEAT_CTRL_OK) {
                printf("WARN!! Frame could not be processes correctly!");
            }
        }

        if (frame.can_id == CAN_SECU1_STAT_FRAME_ID)
        {
            frame2 = true;
            if (handle_secu1_stat(ctx, &frame) != SEAT_CTRL_OK) {
                printf("WARN!! Frame could not be processes correctly!");
            }
        }
        // need to wait that both frame ids are read once
        if (frame1 && frame2){
            frame1 = false;
            frame2 = false;
            if(!ctx->pos_running && ctx->desired_position != MOTOR_POS_INVALID && !ctx->tilt_running && !ctx->height_running){
                // FIXME: SECUx_CMD1 Movement Status has the same values as SECUX
                printf(SELF_SETPOS "Sending: SECU2_CMD_1 [ motor1_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                        ctx->motor_pos, ctx->desired_position, pos_mov_state_string(ctx->desired_pos_direction), ctx->command_pos_ts);

                if (seatctrl_send_ecu2_cmd1(ctx, ctx->desired_pos_direction, ctx->config.motor_pos_rpm, 1)) {
                    perror(SELF_SETPOS "seatctrl_send_ecu2_cmd1() error");
                    // FIXME: abort operation
                }
                std::lock_guard<std::mutex> lock(ctx_mutex);
                ctx->pos_running = true;
            }else if(!ctx->tilt_running && ctx->desired_tilt != MOTOR_POS_INVALID && !ctx->pos_running && !ctx->height_running){
                // FIXME: SECUx_CMD1 Movement Status has the same values as SECUX
                printf(SELF_SETPOS "Sending: SECU2_CMD_1 [ motor2_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                        ctx->motor_tilt, ctx->desired_tilt, tilt_mov_state_string(ctx->desired_tilt_direction), ctx->command_tilt_ts);

                if (seatctrl_send_ecu2_cmd1(ctx, ctx->desired_tilt_direction, ctx->config.motor_tilt_rpm, 3) < 0) {
                    perror(SELF_SETPOS "seatctrl_send_ecu2_cmd1() error");
                    // FIXME: abort operation
                }
                std::lock_guard<std::mutex> lock(ctx_mutex);
                ctx->tilt_running = true;
            }else if(!ctx->height_running && ctx->desired_height != MOTOR_POS_INVALID && !ctx->pos_running && !ctx->tilt_running){
                // FIXME: SECUx_CMD1 Movement Status has the same values as SECUX
                printf(SELF_SETPOS "Sending: SECU1_CMD_1 [ motor3_pos: %d%%, desired_pos: %d%%, dir: %s ] ts: %" PRId64 "\n",
                        ctx->motor_height, ctx->desired_height, height_mov_state_string(ctx->desired_height_direction), ctx->command_height_ts);

                if (seatctrl_send_ecu1_cmd1(ctx, ctx->desired_height_direction, ctx->config.motor_height_rpm, 1) < 0) {
                    perror(SELF_SETPOS "seatctrl_send_ecu1_cmd1() error");
                    // FIXME: abort operation
                }
                std::lock_guard<std::mutex> lock(ctx_mutex);
                ctx->height_running = true;
            }else{
                seatctrl_control_ecu12_loop(ctx);
            }
        }
        ::usleep(1000);
    }

    if (ctx->config.debug_verbose) printf(PREFIX_CTL "Thread stopped.\n");
    return NULL;
}


/**
 * @brief Sends an CAN_secu2_cmd_1_t to SocketCAN
 *
 * @param ctx SeatCtrl context
 * @param motor_dir motor move direction: value of MotorPosDirection or MotorTiltDirection enum
 * @param motor_rpm motor RPMs (actually PWM percentage in range [30-100] or 0 to stop movement)
 * @param motor which motor should get used 1, 2, 3, 4
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error
 */
error_t seatctrl_send_ecu2_cmd1(seatctrl_context_t *ctx, uint8_t motor_dir, uint8_t motor_rpm, uint8_t motor)
{
    int rc;
    CAN_secu2_cmd_1_t cmd1;
    struct can_frame frame;
    uint8_t rpm = 0;

    if (ctx->socket == SOCKET_INVALID) {
        printf(SELF_CMD1 "ERR: CAN Socket not available!\n");
        return SEAT_CTRL_ERR;
    }
    // argument check?

    memset(&cmd1, 0, sizeof(CAN_secu2_cmd_1_t));
    // FIXME: range checks! [0..254]
    switch(motor){
        case 1:
            cmd1.motor1_manual_cmd = motor_dir;
            cmd1.motor1_set_rpm = motor_rpm;
            cmd1.motor3_manual_cmd = ctx->motor_tilt_mov_state;
            if(ctx->motor_tilt_mov_state == MotorTiltDirection::TILT_OFF || ctx->motor_tilt_mov_state == MotorTiltDirection::TILT_INV){
                rpm = 0;
            }
            else{
                rpm = ctx->config.motor_tilt_rpm;
            }
            cmd1.motor3_set_rpm = rpm;
            break;
        case 2:
            cmd1.motor2_manual_cmd = motor_dir;
            cmd1.motor2_set_rpm = motor_rpm;
            break;
        case 3:
            cmd1.motor3_manual_cmd = motor_dir;
            cmd1.motor3_set_rpm = motor_rpm;
            if(ctx->motor_pos_mov_state == MotorPosDirection::POS_OFF || ctx->motor_pos_mov_state == MotorPosDirection::POS_INV){
                rpm = 0;
            }
            else{
                rpm = ctx->config.motor_pos_rpm;
            }
            cmd1.motor1_manual_cmd = ctx->motor_pos_mov_state;
            cmd1.motor1_set_rpm = rpm;
            break;
        case 4:
            cmd1.motor4_manual_cmd = motor_dir;
            cmd1.motor4_set_rpm = motor_rpm;
            break;
        default:
            printf("ERR: Not a valid motor \n");
    }

    memset(&frame, 0, sizeof(struct can_frame));
    frame.can_id = CAN_SECU2_CMD_1_FRAME_ID;
    rc = CAN_secu2_cmd_1_pack(frame.data, &cmd1, sizeof(CAN_secu2_cmd_1_t));
    if (rc < 0) {
        printf(SELF_CMD1 "ERR: CAN_secu2_cmd_1_pack() error\n");
        return SEAT_CTRL_ERR;
    }
    frame.can_dlc = rc; // = rc; BUGFIX: we have to send 5 bytes, regardless of actual CAN_secu2_cmd_1_pack packed size, append 00s
    print_secu2_cmd_1(SELF_CMD1 "*** Sending SECU2_CMD_1: " , &cmd1);
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
 * @brief Sends an CAN_secu1_cmd_1_t to SocketCAN
 *
 * @param ctx SeatCtrl context
 * @param motor_dir motor move direction: value of MotorHeightDirection enum
 * @param motor_rpm motor RPMs (actually PWM percentage in range [30-100] or 0 to stop movement)
 * @param motor which motor should get used 1, 2, 3, 4
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error
 */
error_t seatctrl_send_ecu1_cmd1(seatctrl_context_t *ctx, uint8_t motor_dir, uint8_t motor_rpm, uint8_t motor)
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
    switch(motor){
        case 1:
            cmd1.motor1_manual_cmd = motor_dir;
            cmd1.motor1_set_rpm = motor_rpm;
            break;
        case 2:
            cmd1.motor2_manual_cmd = motor_dir;
            cmd1.motor2_set_rpm = motor_rpm;
            break;
        case 3:
            cmd1.motor3_manual_cmd = motor_dir;
            cmd1.motor3_set_rpm = motor_rpm;
            break;
        case 4:
            cmd1.motor4_manual_cmd = motor_dir;
            cmd1.motor4_set_rpm = motor_rpm;
            break;
        default:
            printf("ERR: Not a valid motor \n");
    }

    memset(&frame, 0, sizeof(struct can_frame));
    frame.can_id = CAN_SECU1_CMD_1_FRAME_ID;
    rc = CAN_secu1_cmd_1_pack(frame.data, &cmd1, sizeof(CAN_secu1_cmd_1_t));
    if (rc < 0) {
        printf(SELF_CMD1 "ERR: CAN_secu1_cmd_1_pack() error\n");
        return SEAT_CTRL_ERR;
    }
    frame.can_dlc = rc; // = rc; BUGFIX: we have to send full 8 bytes, regardless of actual CAN_secu1_cmd_1_pack packed size, append 00s
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
error_t seatctrl_stop_pos_movement(seatctrl_context_t *ctx)
{
    printf(SELF_STOPMOV "Sending MotorPosOff command...\n");

    error_t rc = seatctrl_send_ecu2_cmd1(ctx, MotorPosDirection::POS_OFF, 0, 1); // off, 0rpm
    if (rc != SEAT_CTRL_OK) {
        perror(SELF_STOPMOV "seatctrl_send_ecu2_cmd1() error");
        // also invalidate CTL?
    }

    // invalidate states. FIXME: lock with mutex?
    std::lock_guard<std::mutex> lock(ctx_mutex);
    ctx->desired_position = MOTOR_POS_INVALID;
    ctx->desired_pos_direction = MotorPosDirection::POS_OFF;
    ctx->command_pos_ts = 0;
    ctx->pos_running = false;

    return rc;
}

/**
 * @brief See seat_controller.h
 */
error_t seatctrl_stop_tilt_movement(seatctrl_context_t *ctx)
{
    printf(SELF_STOPMOV "Sending MotorTiltOff command...\n");

    error_t rc = seatctrl_send_ecu2_cmd1(ctx, MotorTiltDirection::TILT_OFF, 0, 3); // off, 0rpm
    if (rc != SEAT_CTRL_OK) {
        perror(SELF_STOPMOV "seatctrl_send_ecu2_cmd1() error");
        // also invalidate CTL?
    }

    // invalidate states. FIXME: lock with mutex?
    std::lock_guard<std::mutex> lock(ctx_mutex);
    ctx->desired_tilt = MOTOR_POS_INVALID;
    ctx->desired_tilt_direction = MotorTiltDirection::TILT_OFF;
    ctx->command_tilt_ts = 0;
    ctx->tilt_running = false;

    return rc;
}

/**
 * @brief See seat_controller.h
 */
error_t seatctrl_stop_height_movement(seatctrl_context_t *ctx)
{
    printf(SELF_STOPMOV "Sending MotorHeightOff command...\n");

    error_t rc = seatctrl_send_ecu1_cmd1(ctx, MotorHeightDirection::HEIGHT_OFF, 0, 1); // off, 0rpm
    if (rc != SEAT_CTRL_OK) {
        perror(SELF_STOPMOV "seatctrl_send_ecu1_cmd1() error");
        // also invalidate CTL?
    }

    // invalidate states. FIXME: lock with mutex?
    std::lock_guard<std::mutex> lock(ctx_mutex);
    ctx->desired_height = MOTOR_POS_INVALID;
    ctx->desired_height_direction = MotorHeightDirection::HEIGHT_OFF;
    ctx->command_height_ts = 0;
    ctx->height_running = false;

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

    // FIXME: use pthred_mutex in ctx?

    // sanity checks for incoming can signal states
    if (ctx->motor_pos == MOTOR_POS_INVALID) {
        printf(SELF_SETPOS "WARN: Motor1 position is invalid: %d\n", ctx->motor_pos);
        // wait some more and if still not incoming - bail out with error
        for (int retries = 0; retries < 30; retries++) { // wait up to 3 sec
            if (ctx->motor_pos != MOTOR_POS_INVALID) {
                break;
            }
            usleep(100 * 1000L);
        }
        if (ctx->motor_pos == MOTOR_POS_INVALID) {
            printf(SELF_SETPOS "Check %s interface for incoming SECU2_STAT frames!\n", ctx->config.can_device);
            printf(SELF_SETPOS "Seat Adjustment to %d%% aborted.\n", desired_position);
            return SEAT_CTRL_ERR_NO_FRAMES;
        }
    }
    if (ctx->motor_pos_mov_state != MotorPosDirection::POS_OFF)
    {
        printf(SELF_SETPOS "WARN: Motor1 status is %s\n", pos_mov_state_string(ctx->motor_pos_mov_state));
    }

    if (is_ctl_running(ctx) && ctx->desired_position != desired_position)
    {
        printf(SELF_SETPOS "WARN: Overriding previous motor1_pos[%d] with new value:[%d]\n", ctx->desired_position, desired_position);
    }
    // BUGFIX: always send motor off command
    rc = seatctrl_stop_pos_movement(ctx);
    usleep(100 * 1000L);
    //if (ctx->desired_position != MOTOR_POS_INVALID && ctx->desired_position != desired_position || ctx->motor_pos_mov_state != MotorPosDirection::POS_OFF)

    int current_pos = ctx->motor_pos;
    if (current_pos == desired_position) {
        printf(SELF_SETPOS "*** Already at requested position: %d%%\n", desired_position);
        if (ctx->motor_pos_mov_state != MotorPosDirection::POS_OFF) {
            rc = seatctrl_stop_pos_movement(ctx);
        } else {
            ctx->desired_pos_direction = MotorPosDirection::POS_OFF;
            ctx->desired_position = MOTOR_POS_INVALID;
            ctx->command_pos_ts = 0;
        }
        return SEAT_CTRL_OK;
    }

    // calculate desired direction based on last known position

    MotorPosDirection direction = MotorPosDirection::POS_INV;
    if (current_pos < desired_position) {
        direction = MotorPosDirection::POS_INC;
    } else {
        direction = MotorPosDirection::POS_DEC;
    }
    // sync!
    ctx->command_pos_ts = get_ts();
    ctx->desired_pos_direction = direction;
    ctx->desired_position = desired_position;
    print_ctl_pos_stat(ctx, SELF_SETPOS);

// #if 0 // EXPERIMENTAL
//     // TODO: evaluate ctx->motor_pos and states to check direction
//     for (int i=0; i<500; i++) {
//         usleep(1*1000L); // give motor some time to start moving, then set ctl_active flag
//         if (ctx->motor_pos_mov_state != MotorPosDirection::POS_OFF) {
//             int64_t elapsed = ctx->command_pos_ts != 0 ? get_ts() - ctx->command_pos_ts : -1;
//             printf(SELF_SETPOS "Motor movement detected [ motor_pos: %d%%, dir: %s ] in %" PRId64 "ms.\n",
//                 ctx->motor_pos, pos_mov_state_string(ctx->motor_pos_mov_state), elapsed);
//             break;
//         }
//     }
// #endif
    return rc;
}

/**
 * @brief See seat_controller.h
 */
error_t seatctrl_set_tilt(seatctrl_context_t *ctx, int32_t desired_tilt)
{
    error_t rc = 0;
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf(SELF_SETTILT "ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    // abort if current pos / directions are unknown
    printf("\n" SELF_SETTILT "Seat Adjustment requested for position: %d%%.\n", desired_tilt);
    if (desired_tilt < 0 || desired_tilt > 100) {
        printf(SELF_SETTILT "ERR: Invalid position!\n");
        return SEAT_CTRL_ERR_INVALID;
    }

    // FIXME: use pthred_mutex in ctx?

    // sanity checks for incoming can signal states
    if (ctx->motor_tilt == MOTOR_POS_INVALID) {
        printf(SELF_SETTILT "WARN: Motor2 position is invalid: %d\n", ctx->motor_tilt);
        // wait some more and if still not incoming - bail out with error
        for (int retries = 0; retries < 30; retries++) { // wait up to 3 sec
            if (ctx->motor_tilt != MOTOR_POS_INVALID) {
                break;
            }
            usleep(100 * 1000L);
        }
        if (ctx->motor_tilt == MOTOR_POS_INVALID) {
            printf(SELF_SETTILT "Check %s interface for incoming SECU2_STAT frames!\n", ctx->config.can_device);
            printf(SELF_SETTILT "Seat Adjustment to %d%% aborted.\n", desired_tilt);
            return SEAT_CTRL_ERR_NO_FRAMES;
        }
    }
    if (ctx->motor_tilt_mov_state != MotorTiltDirection::TILT_OFF)
    {
        printf(SELF_SETTILT "WARN: Motor2 status is %s\n", tilt_mov_state_string(ctx->motor_tilt_mov_state));
    }

    if (is_ctl_running(ctx) && ctx->desired_tilt != desired_tilt)
    {
        printf(SELF_SETTILT "WARN: Overriding previous motor2_pos[%d] with new value:[%d]\n", ctx->desired_tilt, desired_tilt);
    }
    // BUGFIX: always send motor off command
    rc = seatctrl_stop_tilt_movement(ctx);
    usleep(100 * 1000L);
    //if (ctx->desired_tilt != MOTOR_POS_INVALID && ctx->desired_tilt != desired_tilt || ctx->motor_pos_mov_state != MotorTiltDirection::TILT_OFF)

    int current_pos = ctx->motor_tilt;
    if (current_pos == desired_tilt) {
        printf(SELF_SETTILT "*** Already at requested position: %d%%\n", desired_tilt);
        if (ctx->motor_tilt_mov_state != MotorTiltDirection::TILT_OFF) {
            rc = seatctrl_stop_tilt_movement(ctx);
        } else {
            ctx->desired_tilt_direction = MotorTiltDirection::TILT_OFF;
            ctx->desired_tilt = MOTOR_POS_INVALID;
            ctx->command_tilt_ts = 0;
        }
        return SEAT_CTRL_OK;
    }

    // calculate desired direction based on last known position

    MotorTiltDirection direction = MotorTiltDirection::TILT_INV;
    if (current_pos < desired_tilt) {
        direction = MotorTiltDirection::TILT_INC;
    } else {
        direction = MotorTiltDirection::TILT_DEC;
    }
    // sync!
    ctx->command_tilt_ts = get_ts();
    ctx->desired_tilt_direction = direction;
    ctx->desired_tilt = desired_tilt;
    print_ctl_tilt_stat(ctx, SELF_SETTILT);

// #if 0 // EXPERIMENTAL
//     // TODO: evaluate ctx->motor_pos and states to check direction
//     for (int i=0; i<500; i++) {
//         usleep(1*1000L); // give motor some time to start moving, then set ctl_active flag
//         if (ctx->motor_tilt_mov_state != MotorTiltDirection::TILT_OFF) {
//             int64_t elapsed = ctx->command_tilt_ts != 0 ? get_ts() - ctx->command_tilt_ts : -1;
//             printf(SELF_SETTILT "Motor movement detected [ motor_pos: %d%%, dir: %s ] in %" PRId64 "ms.\n",
//                 ctx->motor_tilt, tilt_mov_state_string(ctx->motor_tilt_mov_state), elapsed);
//             break;
//         }
//     }
// #endif
    return rc;
}

/**
 * @brief See seat_controller.h
 */
error_t seatctrl_set_height(seatctrl_context_t *ctx, int32_t desired_height)
{
    error_t rc = 0;
    if (!ctx || ctx->magic != SEAT_CTRL_CONTEXT_MAGIC) {
        printf(SELF_SETHEIGHT "ERR: Invalid context!\n");
        return SEAT_CTRL_ERR_INVALID;
    }
    // abort if current pos / directions are unknown
    printf("\n" SELF_SETHEIGHT "Seat Adjustment requested for position: %d%%.\n", desired_height);
    if (desired_height < 0 || desired_height > 100) {
        printf(SELF_SETHEIGHT "ERR: Invalid position!\n");
        return SEAT_CTRL_ERR_INVALID;
    }

    // FIXME: use pthred_mutex in ctx?

    // sanity checks for incoming can signal states
    if (ctx->motor_height == MOTOR_POS_INVALID) {
        printf(SELF_SETHEIGHT "WARN: Motor3 position is invalid: %d\n", ctx->motor_height);
        // wait some more and if still not incoming - bail out with error
        for (int retries = 0; retries < 30; retries++) { // wait up to 3 sec
            if (ctx->motor_height != MOTOR_POS_INVALID) {
                break;
            }
            usleep(100 * 1000L);
        }
        if (ctx->motor_height == MOTOR_POS_INVALID) {
            printf(SELF_SETHEIGHT "Check %s interface for incoming SECU2_STAT frames!\n", ctx->config.can_device);
            printf(SELF_SETHEIGHT "Seat Adjustment to %d%% aborted.\n", desired_height);
            return SEAT_CTRL_ERR_NO_FRAMES;
        }
    }
    if (ctx->motor_height_mov_state != MotorHeightDirection::HEIGHT_OFF)
    {
        printf(SELF_SETHEIGHT "WARN: Motor3 status is %s\n", height_mov_state_string(ctx->motor_height_mov_state));
    }

    if (is_ctl_running(ctx) && ctx->desired_height != desired_height)
    {
        printf(SELF_SETHEIGHT "WARN: Overriding previous motor3_pos[%d] with new value:[%d]\n", ctx->desired_height, desired_height);
    }
    // BUGFIX: always send motor off command
    rc = seatctrl_stop_height_movement(ctx);
    usleep(100 * 1000L);
    //if (ctx->desired_height != MOTOR_POS_INVALID && ctx->desired_height != desired_height || ctx->motor_height_mov_state != MotorHeightDirection::HEIGHT_OFF)
    
    int current_pos = ctx->motor_height;
    if (current_pos == desired_height) {
        printf(SELF_SETHEIGHT "*** Already at requested position: %d%%\n", desired_height);
        if (ctx->motor_height_mov_state != MotorHeightDirection::HEIGHT_OFF) {
            rc = seatctrl_stop_height_movement(ctx);
        } else {
            ctx->desired_height_direction = MotorHeightDirection::HEIGHT_OFF;
            ctx->desired_height = MOTOR_POS_INVALID;
            ctx->command_height_ts = 0;
        }
        return SEAT_CTRL_OK;
    }

    // calculate desired direction based on last known position

    MotorHeightDirection direction = MotorHeightDirection::HEIGHT_INV;
    if (current_pos < desired_height) {
        direction = MotorHeightDirection::HEIGHT_INC;
    } else {
        direction = MotorHeightDirection::HEIGHT_DEC;
    }
    // sync!
    ctx->command_height_ts = get_ts();
    ctx->desired_height_direction = direction;
    ctx->desired_height = desired_height;
    print_ctl_height_stat(ctx, SELF_SETHEIGHT);

// #if 0 // EXPERIMENTAL
//     // TODO: evaluate ctx->motor_height and states to check direction
//     for (int i=0; i<500; i++) {
//         usleep(1*1000L); // give motor some time to start moving, then set ctl_active flag
//         if (ctx->motor_height_mov_state != MotorHeightDirection::HEIGHT_OFF) {
//             int64_t elapsed = ctx->command_height_ts != 0 ? get_ts() - ctx->command_height_ts : -1;
//             printf(SELF_SETHEIGHT "Motor movement detected [ motor_pos: %d%%, dir: %s ] in %" PRId64 "ms.\n",
//                 ctx->motor_height, height_mov_state_string(ctx->motor_height_mov_state), elapsed);
//             break;
//         }
//     }
// #endif
    return rc;
}


/**
 * @brief See seat_controller.h
 */
error_t seatctrl_default_config(seatctrl_config_t *config)
{
    if (!config) return SEAT_CTRL_ERR_INVALID;

    error_t ret = SEAT_CTRL_OK;

    // default values
    memset(config, 0, sizeof(seatctrl_config_t));
    config->can_device = "can0";
    config->debug_raw = false;
    config->debug_ctl = true;
    config->debug_stats = true;
    config->debug_verbose = false;
    config->motor_height_rpm = DEFAULT_HEIGHT_RPM; // WARNING! uint8_t !!!
    config->motor_tilt_rpm = DEFAULT_TILT_RPM; // WARNING! uint8_t !!!
    config->motor_pos_rpm = DEFAULT_POS_RPM; // WARNING! uint8_t !!!
    config->command_timeout = DEFAULT_OPERATION_TIMEOUT;

    if (getenv("SC_CAN")) config->can_device = getenv("SC_CAN");

    if (getenv("SC_RAW")) config->debug_raw = atoi(getenv("SC_RAW"));
    if (getenv("SC_CTL")) config->debug_ctl = atoi(getenv("SC_CTL"));
    if (getenv("SC_STAT")) config->debug_stats = atoi(getenv("SC_STAT"));
    if (getenv("SC_VERBOSE")) config->debug_verbose = atoi(getenv("SC_VERBOSE"));

    if (getenv("SC_TILT_RPM")) config->motor_tilt_rpm = atoi(getenv("SC_TILT_RPM"));
    if (getenv("SC_POS_RPM")) config->motor_pos_rpm = atoi(getenv("SC_POS_RPM"));
    if (getenv("SC_HEIGHT_RPM")) config->motor_height_rpm = atoi(getenv("SC_HEIGHT_RPM"));
    if (getenv("SC_TIMEOUT")) config->command_timeout = atoi(getenv("SC_TIMEOUT"));

    printf("### seatctrl_config: { can:%s, motor_height_rpm:%d, operation_timeout:%d }\n",
            config->can_device, config->motor_height_rpm, config->command_timeout);
    printf("### seatctrl_config: { can:%s, motor_tilt_rpm:%d, operation_timeout:%d }\n",
            config->can_device, config->motor_tilt_rpm, config->command_timeout);
    printf("### seatctrl_config: { can:%s, motor_pos_rpm:%d, operation_timeout:%d }\n",
            config->can_device, config->motor_pos_rpm, config->command_timeout);
    printf("### seatctrl_logs  : { raw:%d, ctl:%d, stat:%d, verb:%d }\n",
            config->debug_raw, config->debug_ctl, config->debug_stats, config->debug_verbose);
    // args check:
    if (config->motor_pos_rpm < 1 || config->motor_pos_rpm > 254) {
        printf("### SC_POS_RPM: %d, range is [1..254]\n", config->motor_pos_rpm);
        config->motor_pos_rpm = DEFAULT_POS_RPM;
        ret = SEAT_CTRL_ERR_INVALID;
    }

    if (config->motor_tilt_rpm < 1 || config->motor_tilt_rpm > 254) {
        printf("### SC_TLT_RPM: %d, range is [1..254]\n", config->motor_tilt_rpm);
        config->motor_tilt_rpm = DEFAULT_TILT_RPM;
        ret = SEAT_CTRL_ERR_INVALID;
    }

    if (config->motor_height_rpm < 1 || config->motor_height_rpm > 254) {
        printf("### SC_HEIGHT_RPM: %d, range is [1..254]\n", config->motor_height_rpm);
        config->motor_height_rpm = DEFAULT_HEIGHT_RPM;
        ret = SEAT_CTRL_ERR_INVALID;
    }
    
    return ret;
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
    ctx->desired_pos_direction = MotorPosDirection::POS_OFF;

    ctx->desired_tilt = MOTOR_POS_INVALID;
    ctx->desired_tilt_direction = MotorTiltDirection::TILT_OFF;

    ctx->desired_height = MOTOR_POS_INVALID;
    ctx->desired_height_direction = MotorHeightDirection::HEIGHT_OFF;

    ctx->motor_pos_mov_state = MotorPosDirection::POS_INV;
    ctx->motor_pos_learning_state = PosLearningState::PosInvalid;
    ctx->motor_pos = MOTOR_POS_INVALID; // haven't been read yet, invalid(-1)=not learned(255)

    ctx->motor_tilt_mov_state = MotorTiltDirection::TILT_INV;
    ctx->motor_tilt_learning_state = TiltLearningState::TiltInvalid;
    ctx->motor_tilt = MOTOR_POS_INVALID; // haven't been read yet, invalid(-1)=not learned(255)

    ctx->motor_height_mov_state = MotorHeightDirection::HEIGHT_INV;
    ctx->motor_height_learning_state = HeightLearningState::HeightInvalid;
    ctx->motor_height = MOTOR_POS_INVALID; // haven't been read yet, invalid(-1)=not learned(255)

    // invalidate for seatctrl_open()
    ctx->socket = SOCKET_INVALID;
    ctx->thread_id = (pthread_t)0;
    ctx->event_cb = NULL;
    ctx->event_cb_user_data = NULL;
    ctx->pos_running = false;
    ctx->tilt_running = false;
    ctx->height_running = false;

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

    // FIXME: wait some time and check if SECU2_STAT signals are incoming from the thread
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
