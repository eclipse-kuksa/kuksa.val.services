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
 * @file      seat_controller.h
 * @brief     File contains seat controller declarations
 */

#ifndef SEAT_CTRL_CONTROLLER_H
#define SEAT_CTRL_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

// cantools generated code from .dbc
#include "CAN.h"

/**
 * error_t constants
 */

/** No Error */
#define SEAT_CTRL_OK             0

/** Generic error */
#define SEAT_CTRL_ERR           -1

/* SocketCAN not available */
#define SEAT_CTRL_ERR_NO_CAN    -2

/* CAN interface index error */
#define SEAT_CTRL_ERR_IFR       -3

/* SocketCAN bind() error */
#define SEAT_CTRL_ERR_CAN_BIND  -4

/* SocketCAN i/o error */
#define SEAT_CTRL_ERR_CAN_IO    -5

/** Invalid arguments (-EINVAL) */
#define SEAT_CTRL_ERR_INVALID   -EINVAL

/** Can signals not coming from ECU */
#define SEAT_CTRL_ERR_NO_FRAMES -42



/**
 * @brief If defined, dump / handle all 4 motors
 */
// #define SEAT_CTRL_ALL_MOTORS

/**
 * @brief Invalid value for seatctrl_context_t.socket
 */
#define SOCKET_INVALID 				-1

/**
 * @brief Invalid motor position% value in dbc. 255=motor position not learned
 */
#define MOTOR_POS_INVALID 			0xFF

/**
 * @brief Default RPMs for motor. 80=8000 rpm.
 * NOTE: Current firmware threads RPM value as PWM in range [30..100%]. RPM<30 do not move the motor!
 */
#define DEFAULT_HEIGHT_RPM					80

/**
 * @brief Default RPMs for motor. 30=3000 rpm.
 * NOTE: Current firmware threads RPM value as PWM in range [30..100%]. RPM<30 do not move the motor!
 */
#define DEFAULT_TILT_RPM					48

/**
 * @brief Default RPMs for motor. 30=3000 rpm.
 * NOTE: Current firmware threads RPM value as PWM in range [30..100%]. RPM<30 do not move the motor!
 */
#define DEFAULT_POS_RPM					48

/**
 * @brief Default timeout for aborting seatctrl_set_position() if desired position is not reached.
 */
#define DEFAULT_OPERATION_TIMEOUT	15000

/**
 * @brief Special value set in seatctrl_init_ctx() to identify valid context memory
 */
#define SEAT_CTRL_CONTEXT_MAGIC			0xDEC0DE00

/**
 * @brief Error value from SEAT_CTRL_XXX defines
 */
typedef int error_t;

enum SeatCtrlEvent { CanError, MotorPos, MotorTilt, MotorHeight };

/**
 * @brief SeatController Event callback (Motor position changed, CAN Errors)
 * NOTE: value is reused as can error code, motor1 pos.
 */
typedef void (*seatctrl_event_cb_t)(SeatCtrlEvent type, int value, void* userContext);

/**
 * @brief enum for CAN_secu2_cmd_1_t.motor1_manual_cmd
 *
 */
enum MotorPosDirection {
	POS_OFF = CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_OFF_CHOICE, // CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_OFF_CHOICE, CAN_SECU2_STAT_MOTOR1_MOV_STATE_OFF_CHOICE
	POS_DEC = CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_DEC_CHOICE, // CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_DEC_CHOICE, CAN_SECU2_STAT_MOTOR1_MOV_STATE_DEC_CHOICE
	POS_INC = CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_INC_CHOICE, // CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_INC_CHOICE, CAN_SECU2_STAT_MOTOR1_MOV_STATE_INC_CHOICE
	POS_INV = CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_INV_CHOICE	 // CAN_SECU2_CMD_1_MOTOR1_MANUAL_CMD_INV_CHOICE, CAN_SECU2_STAT_MOTOR1_MOV_STATE_DEF_CHOICE
};

/**
 * @brief enum for CAN_secu2_stat_t.motor1_mov_state
 *
 */
enum RecMotorPosDirection {
	REC_POS_OFF = CAN_SECU2_STAT_MOTOR1_MOV_STATE_OFF_CHOICE,
	REC_POS_DEC = CAN_SECU2_STAT_MOTOR1_MOV_STATE_DEC_CHOICE,
	REC_POS_INC = CAN_SECU2_STAT_MOTOR1_MOV_STATE_INC_CHOICE,
	REC_POS_INV = CAN_SECU2_STAT_MOTOR1_MOV_STATE_DEF_CHOICE
};

/**
 * @brief enum for CAN_secu2_cmd_1_t.motor3_manual_cmd
 *
 */
enum MotorTiltDirection {
	TILT_OFF = CAN_SECU2_CMD_1_MOTOR3_MANUAL_CMD_OFF_CHOICE,
	TILT_DEC = CAN_SECU2_CMD_1_MOTOR3_MANUAL_CMD_DEC_CHOICE,
	TILT_INC = CAN_SECU2_CMD_1_MOTOR3_MANUAL_CMD_INC_CHOICE,
	TILT_INV = CAN_SECU2_CMD_1_MOTOR3_MANUAL_CMD_INV_CHOICE	
};

/**
 * @brief enum for CAN_secu2_stat_t.motor3_mov_state
 *
 */
enum RecMotorTiltDirection {
	REC_TILT_OFF = CAN_SECU2_STAT_MOTOR3_MOV_STATE_OFF_CHOICE,
	REC_TILT_DEC = CAN_SECU2_STAT_MOTOR3_MOV_STATE_DEC_CHOICE,
	REC_TILT_INC = CAN_SECU2_STAT_MOTOR3_MOV_STATE_INC_CHOICE,
	REC_TILT_INV = CAN_SECU2_STAT_MOTOR3_MOV_STATE_DEF_CHOICE
};


/**
 * @brief Common enum for CAN_secu1_cmd_1_t.motor1_manual_cmd and CAN_secu1_stat_t.motor1_mov_state
 *
 * @fixme: Use cantools generated constants per CAN ID in case those are different in the future
 */
enum MotorHeightDirection {
	HEIGHT_OFF = CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_OFF_CHOICE, // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_OFF_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_OFF_CHOICE
	HEIGHT_DEC = CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_DEC_CHOICE, // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_DEC_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_DEC_CHOICE
	HEIGHT_INC = CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_INC_CHOICE, // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_INC_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_INC_CHOICE
	HEIGHT_INV = CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_INV_CHOICE	// CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_INV_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_DEF_CHOICE
};

/**
 * @brief Motor learning state values.
 */
enum PosLearningState {
	PosNotLearned = CAN_SECU2_STAT_MOTOR1_LEARNING_STATE_NOT_LEARNED_CHOICE,
	PosLearned = CAN_SECU2_STAT_MOTOR1_LEARNING_STATE_LEARNED_CHOICE,
	PosInvalid = CAN_SECU2_STAT_MOTOR1_LEARNING_STATE_INVALID_CHOICE
};

/**
 * @brief Motor learning state values.
 */
enum TiltLearningState {
	TiltNotLearned = CAN_SECU2_STAT_MOTOR3_LEARNING_STATE_NOT_LEARNED_CHOICE,
	TiltLearned = CAN_SECU2_STAT_MOTOR3_LEARNING_STATE_LEARNED_CHOICE,
	TiltInvalid = CAN_SECU2_STAT_MOTOR3_LEARNING_STATE_INVALID_CHOICE
};

/**
 * @brief Motor learning state values.
 */
enum HeightLearningState {
	HeightNotLearned = CAN_SECU1_STAT_MOTOR1_LEARNING_STATE_NOT_LEARNED_CHOICE,
	HeightLearned = CAN_SECU1_STAT_MOTOR1_LEARNING_STATE_LEARNED_CHOICE,
	HeightInvalid = CAN_SECU1_STAT_MOTOR1_LEARNING_STATE_INVALID_CHOICE
};

/**
 * @brief Customize debug dumps from different CTL sub-modules
 *
 * @param can_device "can0", "vcan0", etc. please use literal values or allocated memory!
 * @param debug_raw dump raw can bytes
 * @param debug_ctl dumps when control loop is handling set_position command
 * @param debug_stats periodic dumps of current SECUx_STAT parsed values
 * @param debug_verbose enable for troubleshooting only
 * @param command_timeout manual command tieout (ms). Moving is stopped after timeout if position not reached
 * @param motor_height_rpm manual command raw rpm/100. [0..254]
 * @param motor_tilt_rpm manual command raw rpm/100. [0..254]
 * @param motor_pos_rpm manual command raw rpm/100. [0..254]
 */
typedef struct {
	const char *can_device; // "can0", "vcan0", etc. please use literal values or allocated memory!
	bool debug_raw;         // dump raw can bytes
	bool debug_ctl;         // dumps when control loop is handling set_position command
	bool debug_stats;       // periodic dumps of current SECUx_STAT parsed values
	bool debug_verbose;     // enable for troubleshooting only
	int  command_timeout;   // manual command tieout (ms). Moving is stopped after timeout if position not reached
	int  motor_height_rpm;         // manual command raw rpm/100. [0..254]
	int  motor_tilt_rpm;
	int  motor_pos_rpm;	
} seatctrl_config_t;

/**
 * @brief seatctrl context structure. Required for seatctrl calls.
 * Must be initialized with seatctrl_init_ctx() first.
 *
 * @param magic Must be #SEAT_CTRL_CONTEXT_MAGIC to consider seatctrl_context_t* valid.
 * @param config seatctrl_config_t config structure.
 * @param socket SocketCAN for CTL. (internal)
 * @param running Flag for running CTL. (internal)
 * @param thread_id ThreadID of the CTL handler thread. (internal)
 * @param command_ts Timestamp when manual command was sent. (internal)
 *
 * @param desired_position Desired target motor position for active operation. (internal)
 * @param desired_tilt Desired target motor position for active operation. (internal)
 * @param desired_height Desired target motor position for active operation. (internal)
 * @param desired_pos_direction Calculated direction of movement towards desired_position. (internal)
 * @param desired_tilt_direction Calculated direction of movement towards desired_position. (internal)
 * @param desired_height_direction Calculated direction of movement towards desired_position. (internal)
 *
 * @param motor_pos Last received (valid) value from CAN_secu2_stat_t.motor1_pos
 * @param motor_pos_mov_state Last received (valid) value from CAN_secu2_stat_t.motor1_mov_state
 * @param motor_pos_learning_state Last Received (valid) value from CAN_secu2_stat_t.motor1_learning_state
 * @param motor_tilt Last received (valid) value from CAN_secu2_stat_t.motor3_pos
 * @param motor_tilt_mov_state Last received (valid) value from CAN_secu2_stat_t.motor3_mov_state
 * @param motor_tilt_learning_state Last Received (valid) value from CAN_secu2_stat_t.motor3_learning_state
 * @param motor_height Last received (valid) value from CAN_secu1_stat_t.motor1_pos
 * @param motor_height_mov_state Last received (valid) value from CAN_secu1_stat_t.motor1_mov_state
 * @param motor_height_learning_state Last Received (valid) value from CAN_secu1_stat_t.motor1_learning_state
 *
 * @param event_cb Callback function (seatctrl_event_cb_t) for motor position changes.
 * @param event_cb_tilt Callback function (seatctrl_event_cb_t) for motor position changes.
 * @param event_cb_height Callback function (seatctrl_event_cb_t) for motor position changes.
 * @param event_cb_user_data Callback function for motor position change user context*.
 */
typedef struct
{
	uint32_t magic;             // Must be #SEAT_CTRL_CONTEXT_MAGIC to consider seatctrl_context_t* valid
	seatctrl_config_t config;   // seatctrl_config_t config structure (copied on init)
	int socket;                 // SocketCAN for CTL
	bool running;               // Flag for running CTL
	pthread_t thread_id;        // ThreadID of the CTL handler thread

	int64_t command_pos_ts;         // Timestamp when manual command was sent
	int64_t command_tilt_ts;         // Timestamp when manual command was sent
	int64_t command_height_ts;         // Timestamp when manual command was sent
	uint8_t desired_position;   // Desired target motor position for active operation
	uint8_t desired_tilt;   // Desired target motor position for active operation
	uint8_t desired_height;   // Desired target motor position for active operation
	MotorPosDirection desired_pos_direction; // Calculated direction of movement towards desired_position
	MotorTiltDirection desired_tilt_direction; // Calculated direction of movement towards desired_position
	MotorHeightDirection desired_height_direction; // Calculated direction of movement towards desired_position

	bool pos_running;					// position adjustment running
	bool tilt_running;					// tilt adjustment running
	bool height_running;				// height adjustment running

	// motor*_* fields below are updated from CAN_SECU2_STAT signal on state change:
	uint8_t motor_pos;            // Last received (valid) value from CAN_secu2_stat_t.motor1_pos
	uint8_t motor_pos_mov_state;      // Last received (valid) value from CAN_secu2_stat_t.motor1_mov_state
	uint8_t motor_pos_learning_state; // Last received (valid) value from CAN_secu2_stat_t.motor1_learning_state

	uint8_t motor_tilt;            // Last received (valid) value from CAN_secu2_stat_t.motor3_pos
	uint8_t motor_tilt_mov_state;      // Last received (valid) value from CAN_secu2_stat_t.motor3_mov_state
	uint8_t motor_tilt_learning_state; // Last received (valid) value from CAN_secu2_stat_t.motor3_learning_state

	uint8_t motor_height;            // Last received (valid) value from CAN_secu1_stat_t.motor1_pos
	uint8_t motor_height_mov_state;      // Last received (valid) value from CAN_secu1_stat_t.motor1_mov_state
	uint8_t motor_height_learning_state; // Last received (valid) value from CAN_secu1_stat_t.motor1_learning_state

	// Callback for position changes
	seatctrl_event_cb_t event_cb;  // Callback function for motor position changes.
	seatctrl_event_cb_t event_cb_tilt;  // Callback function for motor position changes.
	seatctrl_event_cb_t event_cb_height;  // Callback function for motor position changes.
	void* event_cb_user_data; // Callback function for motor position change user context*.

} seatctrl_context_t;

//////////////////////
////  public API  ////
//////////////////////

/**
 * @brief Initializes provided seatctrl_config_t* with default values (supports overriding via getenv()).
 *
 * @param config seatctrl_config_t* structure to be initialized.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_default_config(seatctrl_config_t *config);

/**
 * @brief Initialize seatctrl context with specified seatctrl config.
 *
 * @param ctx seatctrl context to init.
 * @param config already initialized seatctrl_config_t* structure.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_init_ctx(seatctrl_context_t *ctx, seatctrl_config_t *config);

/**
 * @brief Opens CAN socket and starts control loop, must follow seatctrl_init_ctx() call.
 *
 * @param ctx initialized seatctrl context.
 * @return error_t:
 *         - SEAT_CTRL_OK: on success.
 *         - SEAT_CTRL_ERR: generic error.
 *         - SEAT_CTRL_ERR_NO_FRAMES:  Motor1 position is invalid (probably not learned or no CAN signals are coming, e.g. missing hw, sim).
 *         - SEAT_CTRL_ERR_INVALID: invalid arguments.
 */
error_t seatctrl_open(seatctrl_context_t *ctx);

/**
 * @brief Main business logic, sends command to change seat position based on current position and desired_position.
 * Must follow a successful seatctrl_open() call.
 *
 * @param ctx opened seatctrl context.
 * @param desired_position motor1 absolute position(%). Range is [0%..100%], 255=invalid position.
 * @return error_t
 *         - SEAT_CTRL_OK: on success.
 *         - SEAT_CTRL_ERR_NO_FRAMES:  Motor1 position is invalid (probably not learned or no CAN signals are coming, e.g. missing hw, sim).
 *         - SEAT_CTRL_ERR_INVALID: invalid arguments.
 *         - SEAT_CTRL_ERR: generic error.
 */
error_t seatctrl_set_position(seatctrl_context_t *ctx, int32_t desired_position);

/**
 * @brief Main business logic, sends command to change seat position based on current position and desired_position.
 * Must follow a successful seatctrl_open() call.
 *
 * @param ctx opened seatctrl context.
 * @param desired_position motor1 absolute position(%). Range is [0%..100%], 255=invalid position.
 * @return error_t
 *         - SEAT_CTRL_OK: on success.
 *         - SEAT_CTRL_ERR_NO_FRAMES:  Motor1 position is invalid (probably not learned or no CAN signals are coming, e.g. missing hw, sim).
 *         - SEAT_CTRL_ERR_INVALID: invalid arguments.
 *         - SEAT_CTRL_ERR: generic error.
 */
error_t seatctrl_set_tilt(seatctrl_context_t *ctx, int32_t desired_tilt);

/**
 * @brief Main business logic, sends command to change seat position based on current position and desired_position.
 * Must follow a successful seatctrl_open() call.
 *
 * @param ctx opened seatctrl context.
 * @param desired_position motor1 absolute position(%). Range is [0%..100%], 255=invalid position.
 * @return error_t
 *         - SEAT_CTRL_OK: on success.
 *         - SEAT_CTRL_ERR_NO_FRAMES:  Motor1 position is invalid (probably not learned or no CAN signals are coming, e.g. missing hw, sim).
 *         - SEAT_CTRL_ERR_INVALID: invalid arguments.
 *         - SEAT_CTRL_ERR: generic error.
 */
error_t seatctrl_set_height(seatctrl_context_t *ctx, int32_t desired_height);

/**
 * @brief Gets last known motor1 position (%).
 *
 * @param ctx seatctrl context.
 * @return int motor1 absolute position(%). Range is [0%..100%],
 *             #MOTOR_POS_INVALID (255): Unknown motor position.
 *             SEAT_CTRL_ERR: generic error.
 *             SEAT_CTRL_ERR_INVALID: invalid arguments.
 */
int seatctrl_get_position(seatctrl_context_t *ctx);

/**
 * @brief Gets last known motor1 position (%).
 *
 * @param ctx seatctrl context.
 * @return int motor1 absolute position(%). Range is [0%..100%],
 *             #MOTOR_POS_INVALID (255): Unknown motor position.
 *             SEAT_CTRL_ERR: generic error.
 *             SEAT_CTRL_ERR_INVALID: invalid arguments.
 */
int seatctrl_get_tilt(seatctrl_context_t *ctx);

/**
 * @brief Gets last known motor1 position (%).
 *
 * @param ctx seatctrl context.
 * @return int motor1 absolute position(%). Range is [0%..100%],
 *             #MOTOR_POS_INVALID (255): Unknown motor position.
 *             SEAT_CTRL_ERR: generic error.
 *             SEAT_CTRL_ERR_INVALID: invalid arguments.
 */
int seatctrl_get_height(seatctrl_context_t *ctx);

/**
 * @brief Helper to abort any seat active seatctrl_set_position() operations and stop motors.
 *
 * @param ctx opened seatctrl context.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_stop_pos_movement(seatctrl_context_t *ctx);

/**
 * @brief Helper to abort any seat active seatctrl_set_tilt() operations and stop motors.
 *
 * @param ctx opened seatctrl context.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_stop_tilt_movement(seatctrl_context_t *ctx);

/**
 * @brief Helper to abort any seat active seatctrl_set_height() operations and stop motors.
 *
 * @param ctx opened seatctrl context.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_stop_height_movement(seatctrl_context_t *ctx);

/**
 * @brief Set callback function for seatctrl events (e.g. motorX position updates, CAN I/O errors).
 *
 * @param ctx initialized seatctrl context.
 * @param cb callback seatctrl_event_cb_t(SeatCtrlEvent type, int value, void *user_data) function.
 * @param user_data user context, passed as argument to cb.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_set_event_callback(seatctrl_context_t *ctx, seatctrl_event_cb_t cb, void* user_data);

/**
 * @brief Cleanup seatctrl context, stops CTL thread, socket cleanup.
 * After this call, context is invlid for further calls.
 *
 * @param ctx initialized seatctrl context.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_close(seatctrl_context_t *ctx);


#ifdef __cplusplus
}
#endif

#endif
