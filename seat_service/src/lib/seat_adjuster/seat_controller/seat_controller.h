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
#define DEFAULT_RPM					80

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

enum SeatCtrlEvent { CanError, Motor1Pos };

/**
 * @brief SeatController Event callback (Motor position changed, CAN Errors)
 * NOTE: value is reused as can error code, motor1 pos.
 */
typedef void (*seatctrl_event_cb_t)(SeatCtrlEvent type, int value, void* userContext);

/**
 * @brief Common enum for CAN_secu1_cmd_1_t.motor1_manual_cmd and CAN_secu1_stat_t.motor1_mov_state
 *
 * @fixme: Use cantools generated constants per CAN ID in case those are different in the future
 */
enum MotorDirection {
	OFF = 0, // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_OFF_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_OFF_CHOICE
	DEC = 1, // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_DEC_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_DEC_CHOICE
	INC = 2, // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_INC_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_INC_CHOICE
	INV = 3	 // CAN_SECU1_CMD_1_MOTOR1_MANUAL_CMD_INV_CHOICE, CAN_SECU1_STAT_MOTOR1_MOV_STATE_DEF_CHOICE
};

/**
 * @brief Motor learning state values.
 */
enum LearningState {
	NotLearned = 0,
	Learned = 1,
	Invalid = 2
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
 * @param motor_rpm manual command raw rpm/100. [0..254]
 */
typedef struct {
	const char *can_device; // "can0", "vcan0", etc. please use literal values or allocated memory!
	bool debug_raw;         // dump raw can bytes
	bool debug_ctl;         // dumps when control loop is handling set_position command
	bool debug_stats;       // periodic dumps of current SECUx_STAT parsed values
	bool debug_verbose;     // enable for troubleshooting only
	int  command_timeout;   // manual command tieout (ms). Moving is stopped after timeout if position not reached
	int  motor_rpm;         // manual command raw rpm/100. [0..254]
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
 * @param desired_direction Calculated direction of movement towards desired_position. (internal)
 *
 * @param motor1_pos Last received (valid) value from CAN_secu1_stat_t.motor1_pos
 * @param motor1_mov_state Last received (valid) value from CAN_secu1_stat_t.motor1_mov_state
 * @param motor1_learning_state Last Received (valid) value from CAN_secu1_stat_t.motor1_learning_state
 *
 * @param event_cb Callback function (seatctrl_event_cb_t) for motor position changes.
 * @param event_cb_user_data Callback function for motor position change user context*.
 */
typedef struct
{
	uint32_t magic;             // Must be #SEAT_CTRL_CONTEXT_MAGIC to consider seatctrl_context_t* valid
	seatctrl_config_t config;   // seatctrl_config_t config structure (copied on init)
	int socket;                 // SocketCAN for CTL
	bool running;               // Flag for running CTL
	pthread_t thread_id;        // ThreadID of the CTL handler thread

	int64_t command_ts;         // Timestamp when manual command was sent
	uint8_t desired_position;   // Desired target motor position for active operation
	MotorDirection desired_direction; // Calculated direction of movement towards desired_position

	// motor*_* fields below are updated from CAN_SECU1_STAT signal on state change:
	uint8_t motor1_pos;            // Last received (valid) value from CAN_secu1_stat_t.motor1_pos
	uint8_t motor1_mov_state;      // Last received (valid) value from CAN_secu1_stat_t.motor1_mov_state
	uint8_t motor1_learning_state; // Last received (valid) value from CAN_secu1_stat_t.motor1_learning_state

#ifdef SEAT_CTRL_ALL_MOTORS
	uint8_t motor2_pos;            // Last received (valid) value from CAN_secu1_stat_t.motor2_pos
	uint8_t motor2_mov_state;      // Last received (valid) value from CAN_secu1_stat_t.motor2_mov_state
	uint8_t motor2_learning_state; // Last received (valid) value from CAN_secu1_stat_t.motor2_learning_state

	uint8_t motor3_pos;            // Last received (valid) value from CAN_secu1_stat_t.motor3_pos
	uint8_t motor3_mov_state;      // Last received (valid) value from CAN_secu1_stat_t.motor3_mov_state
	uint8_t motor3_learning_state; // Last received (valid) value from CAN_secu1_stat_t.motor3_learning_state

	uint8_t motor4_pos;            // Last received (valid) value from CAN_secu1_stat_t.motor4_pos
	uint8_t motor4_mov_state;      // Last received (valid) value from CAN_secu1_stat_t.motor4_mov_state
	uint8_t motor4_learning_state; // Last received (valid) value from CAN_secu1_stat_t.motor4_learning_state
#endif // # SEAT_CTRL_ALL_MOTORS

	// Callback for position changes
	seatctrl_event_cb_t event_cb;  // Callback function for motor position changes.
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
 * Must follow a successfull seatctrl_open() call.
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
 * @brief Helper to abort any seat active seatctrl_set_position() operations and stop motors.
 *
 * @param ctx opened seatctrl context.
 * @return SEAT_CTRL_OK on success, SEAT_CTRL_ERR* (<0) on error.
 */
error_t seatctrl_stop_movement(seatctrl_context_t *ctx);

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
