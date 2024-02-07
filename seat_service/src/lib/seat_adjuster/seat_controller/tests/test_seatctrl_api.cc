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
 * @file      test_seatctrl_api.cc
 * @brief     File contains 
 */
#include "gtest/gtest.h"

#include "CAN.h"
#include "seat_controller.h"
#include "mock/mock_unix_socket.h"

// forward declare private seat_controller methods
/**
 * @brief
 *
 */
extern int seatctrl_control_ecu12_loop(seatctrl_context_t *ctx);
/**
 * @brief
 *
 */
extern int handle_secu2_stat(seatctrl_context_t *ctx, const struct can_frame *frame);
/**
 * @brief
 *
 */

extern int handle_secu1_stat(seatctrl_context_t *ctx, const struct can_frame *frame);
/**
 * @brief
 *
 */
extern int64_t get_ts();
/**
 * @brief
 *
 */
extern void print_can_raw(const struct can_frame *frame, bool is_received);

namespace sdv {
namespace test {

/**
 * @brief
 *
 */
class TestSeatCtrlApi : public ::testing::Test {

  protected:

    virtual void SetUp() override {
        // fiill in with invalid memory!
        memset(&config, 0xff, sizeof(seatctrl_config_t));
        memset(&ctx, 0xff, sizeof(seatctrl_context_t));

        ResetEnv();
    }

    virtual void TearDown() override {
        // WARN: may fail with random context value here!
        // seatctrl_close(&ctx);
        ResetContext();
    }

    void ResetContext() {
        ctx.magic = SEAT_CTRL_CONTEXT_MAGIC;
        ctx.socket = SOCKET_INVALID;
        ctx.running = false;
        ctx.config.can_device = NULL;
        ctx.thread_id = (pthread_t)NULL;
    }

    void ResetEnv() {
        // make sure environmet variables are unset
        ::unsetenv("SC_CAN");
        ::unsetenv("SC_POS_TIMEOUT");
        ::unsetenv("SC_TILT_TIMEOUT");
        ::unsetenv("SC_HEIGHT_TIMEOUT");
        ::unsetenv("SC_HEIGHT_RPM");
        ::unsetenv("SC_TILT_RPM");
        ::unsetenv("SC_POS_RPM");
        ::unsetenv("SC_RAW");
        ::unsetenv("SC_VERBOSE");
        ::unsetenv("SC_STAT");
        ::unsetenv("SC_CTL");
    }

    /**
     * @brief Generates SECU_STAT can frame with specified position, movement state and learning state for motor1
     *
     * @param frame Pointer to can_frame
     * @param motor_pos desired1 motor pos
     * @param mov_state desired1 movement state
     * @param learn_state desired1 learning state
     */
    int GenerateSecu2StatFrame(struct can_frame *frame, int motor_pos, int mov_state, int learn_state, int motor_tilt, int tilt_mov_state, int tilt_learn_state) {
        if (!frame) return -1;
        ::memset(frame, 0, sizeof(can_frame));

        CAN_secu2_stat_t stat;
        ::memset(&stat, 0, sizeof(CAN_secu2_stat_t));
        stat.motor1_pos = motor_pos;
        stat.motor1_mov_state = mov_state;
        stat.motor1_learning_state = learn_state;

        stat.motor3_pos = motor_tilt;
        stat.motor3_mov_state = tilt_mov_state;
        stat.motor3_learning_state = tilt_learn_state;

        frame->can_id = CAN_SECU2_STAT_FRAME_ID;
        frame->can_dlc = 8;
        int rc = CAN_secu2_stat_pack(frame->data, &stat, sizeof(stat));
        if (rc == 6) {
            return 0;
        } else {
            return 1;
        }
    }

    /**
     * @brief Generates SECU_STAT can frame with specified position, movement state and learning state for motor1
     *
     * @param frame Pointer to can_frame
     * @param motor_pos desired1 motor pos
     * @param mov_state desired1 movement state
     * @param learn_state desired1 learning state
     */
    int GenerateSecu1StatFrame(struct can_frame *frame, int motor_height, int height_mov_state, int learn_state) {
        if (!frame) return -1;
        ::memset(frame, 0, sizeof(can_frame));

        CAN_secu1_stat_t stat;
        ::memset(&stat, 0, sizeof(CAN_secu2_stat_t));
        stat.motor1_pos = motor_height;
        stat.motor1_mov_state = height_mov_state;
        stat.motor1_learning_state = learn_state;

        frame->can_id = CAN_SECU1_STAT_FRAME_ID;
        frame->can_dlc = 8;
        int rc = CAN_secu1_stat_pack(frame->data, &stat, sizeof(stat));
        if (rc == 8) {
            return 0;
        } else {
            return 1;
        }
    }


  protected:
    seatctrl_config_t config;
    seatctrl_context_t ctx;
};


/**
 * @brief Test seatctrl_default_config() default values.
 */
TEST_F(TestSeatCtrlApi, TestConfigDefault) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    // check for SIGSEGV
    EXPECT_EQ(-EINVAL, seatctrl_default_config(nullptr));
    // check for SIGSEGV
    config.can_device = NULL;
    EXPECT_EQ(-EINVAL, seatctrl_default_config(nullptr));

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_STREQ("can0", config.can_device);
    EXPECT_EQ(DEFAULT_HEIGHT_RPM, config.motor_height_rpm);
    EXPECT_EQ(DEFAULT_TILT_RPM, config.motor_tilt_rpm);
    EXPECT_EQ(DEFAULT_POS_RPM, config.motor_pos_rpm);
    EXPECT_EQ(DEFAULT_POS_OPERATION_TIMEOUT, config.command_pos_timeout);
    EXPECT_EQ(DEFAULT_TILT_OPERATION_TIMEOUT, config.command_tilt_timeout);
    EXPECT_EQ(DEFAULT_HEIGHT_OPERATION_TIMEOUT, config.command_height_timeout);
}

/**
 * @brief Test seatctrl_default_config() overide from environment variables
 */
TEST_F(TestSeatCtrlApi, TestConfigEnv) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    ::setenv("SC_CAN", "vcan0", true);
    ::setenv("SC_POS_TIMEOUT", "12345", true);
    ::setenv("SC_TILT_TIMEOUT", "12345", true);
    ::setenv("SC_HEIGHT_TIMEOUT", "12345", true);
    ::setenv("SC_HEIGHT_RPM", "99", true); 
    ::setenv("SC_TILT_RPM", "99", true); 
    ::setenv("SC_POS_RPM", "99", true); 

    ::setenv("SC_RAW", "INVALID", true);
    ::setenv("SC_VERBOSE", "1", true);
    ::setenv("SC_STAT", "1", true);
    ::setenv("SC_CTL", "0", true);

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_STREQ("vcan0", config.can_device);
    EXPECT_EQ(99, config.motor_height_rpm);
    EXPECT_EQ(99, config.motor_tilt_rpm);
    EXPECT_EQ(99, config.motor_pos_rpm);
    EXPECT_EQ(12345, config.command_pos_timeout);
    EXPECT_EQ(12345, config.command_tilt_timeout);
    EXPECT_EQ(12345, config.command_height_timeout);

    EXPECT_EQ(false, config.debug_raw);  // invalid integer=0
    EXPECT_EQ(true, config.debug_verbose);
    EXPECT_EQ(true, config.debug_stats);
    EXPECT_EQ(false, config.debug_ctl);

    // check invalid range for RMP [0..254]
    ::setenv("SC_HEIGHT_RPM", "255", true);                 // invalid value! should reset to default
    ::setenv("SC_TILT_RPM", "255", true);                 // invalid value! should reset to default
    ::setenv("SC_POS_RPM", "255", true);                 // invalid value! should reset to default
    EXPECT_EQ(-EINVAL, seatctrl_default_config(&config));  // should fail
    EXPECT_EQ(DEFAULT_HEIGHT_RPM, config.motor_height_rpm);          // also sets default rpm
    EXPECT_EQ(DEFAULT_TILT_RPM, config.motor_tilt_rpm);          // also sets default rpm
    EXPECT_EQ(DEFAULT_POS_RPM, config.motor_pos_rpm);          // also sets default rpm
}

/**
 * @brief Test seatctrl_init_ctx(), seatctrl_close() normal usage
 */
TEST_F(TestSeatCtrlApi, TestContext) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    EXPECT_EQ(0, seatctrl_default_config(&config));

    // init should succeed
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));
    // sanity checks for context
    EXPECT_EQ(SEAT_CTRL_CONTEXT_MAGIC, ctx.magic);
    EXPECT_EQ(SOCKET_INVALID, ctx.socket);
    EXPECT_EQ(0, (int)ctx.thread_id);

    // check if values from config are copied in ctx.config
    EXPECT_STREQ(config.can_device, ctx.config.can_device);
    EXPECT_EQ(config.motor_height_rpm, ctx.config.motor_height_rpm);
    EXPECT_EQ(config.motor_tilt_rpm, ctx.config.motor_tilt_rpm);
    EXPECT_EQ(config.motor_pos_rpm, ctx.config.motor_pos_rpm);
    EXPECT_EQ(config.command_pos_timeout, ctx.config.command_pos_timeout);
    EXPECT_EQ(config.command_tilt_timeout, ctx.config.command_tilt_timeout);
    EXPECT_EQ(config.command_height_timeout, ctx.config.command_height_timeout);
    EXPECT_EQ(config.debug_ctl, ctx.config.debug_ctl);
    EXPECT_EQ(config.debug_raw, ctx.config.debug_raw);
    EXPECT_EQ(config.debug_stats, ctx.config.debug_stats);
    EXPECT_EQ(config.debug_verbose, ctx.config.debug_verbose);

    // check if current operation is reset
    EXPECT_EQ(ctx.command_pos_ts, 0);
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorPosDirection::POS_OFF, ctx.desired_pos_direction);
    EXPECT_EQ(ctx.command_tilt_ts, 0);
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_tilt);
    EXPECT_EQ(MotorTiltDirection::TILT_OFF, ctx.desired_tilt_direction);
    EXPECT_EQ(ctx.command_height_ts, 0);
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_height);
    EXPECT_EQ(MotorHeightDirection::HEIGHT_OFF, ctx.desired_height_direction);

    // check if initial stats are invalidated
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.motor_pos);
    EXPECT_EQ(MotorPosDirection::POS_INV, ctx.motor_pos_mov_state);
    EXPECT_EQ(PosLearningState::PosInvalid, ctx.motor_pos_learning_state);
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.motor_tilt);
    EXPECT_EQ(MotorTiltDirection::TILT_INV, ctx.motor_tilt_mov_state);
    EXPECT_EQ(TiltLearningState::TiltInvalid, ctx.motor_tilt_learning_state);
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.motor_height);
    EXPECT_EQ(MotorHeightDirection::HEIGHT_INV, ctx.motor_height_mov_state);
    EXPECT_EQ(HeightLearningState::HeightInvalid, ctx.motor_height_learning_state);

    // close after init
    EXPECT_EQ(0, seatctrl_close(&ctx));

    // close on closed context
    EXPECT_EQ(0, seatctrl_close(&ctx));
}

/**
 * @brief Test seatctrl_open() invalid scenarios.
 *
 * NOTE: Happy path requires full mocking of socketcan:
 *       ::socket(), ::bind(), ::read(), ::write()
 */
TEST_F(TestSeatCtrlApi, TestOpenErrors) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    // must fail on invalid mem
    EXPECT_EQ(-EINVAL, seatctrl_open(&ctx)) << "Must fail on invalid context.";

    // check for SIGSEGV
    EXPECT_EQ(-EINVAL, seatctrl_open(nullptr)) << "Should not crash on invalid pointer";

    // initialize normal context
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config)) << "Context must be initialized";

    // context is ok, should fail opening invalid device
    ctx.config.can_device = ".vcan99"; // invalid can, open must fail
    // OK to fail with -2 if SocketCAN is not supported, or -4 if bind failed
    int rc = seatctrl_open(&ctx);
    EXPECT_TRUE(rc == -2 || rc == -4) << "Must fail on socket() or bind()";

    // fail on already connected context:
    ctx.socket = 0xbadc0de;
    ctx.thread_id = 0xbadc0de;
    ctx.running = true;
    EXPECT_EQ(-1, seatctrl_open(&ctx)) << "Must fail on already opened context.";
}

/**
 * @brief Test seatctrl_init_ctx() / seatctrl_close() invalid scenarios
 */
TEST_F(TestSeatCtrlApi, TestContextInvalid) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    // close before init (invalid ctx mem)
    EXPECT_EQ(-EINVAL, seatctrl_close(&ctx));

    // check for SIGSEGV
    EXPECT_EQ(-EINVAL, seatctrl_init_ctx(nullptr, &config));
    EXPECT_EQ(-EINVAL, seatctrl_init_ctx(&ctx, nullptr));

    // check for crash on invalid can_device
    config.can_device = nullptr;
    EXPECT_EQ(-EINVAL, seatctrl_init_ctx(&ctx, &config));

    EXPECT_EQ(-EINVAL, seatctrl_close(nullptr));

    // test long can device length
    config.can_device = "vcan123456789abcef";  // invalid length, max 16
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));
 }

/**
 * @brief Test seatctrl_get_position().
 */
TEST_F(TestSeatCtrlApi, TestGetPosition) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    // check for SIGSEGV
    EXPECT_EQ(-EINVAL, seatctrl_get_position(nullptr));

    // enable raw dumps
    ::setenv("SC_RAW", "1", true);
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    can_frame frame;

    int test_pos = 42;
    int test_tilt = 50;
    int test_height = 35;

    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, test_pos, MotorPosDirection::POS_INV, PosLearningState::PosInvalid, test_tilt, MotorTiltDirection::TILT_INV, TiltLearningState::TiltInvalid)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));

    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame, test_height, MotorHeightDirection::HEIGHT_INV, HeightLearningState::HeightInvalid)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame));

    //ctx.motor_pos = test_pos;
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_position(&ctx)) << "Motor pos should be: " << SEAT_CTRL_ERR << " if CTL thread is not running";
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_tilt(&ctx)) << "Motor pos should be: " << SEAT_CTRL_ERR << " if CTL thread is not running";
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_height(&ctx)) << "Motor pos should be: " << SEAT_CTRL_ERR << " if CTL thread is not running";
    ctx.running = true;
    EXPECT_EQ(test_pos, seatctrl_get_position(&ctx));
    EXPECT_EQ(test_tilt, seatctrl_get_tilt(&ctx));
    EXPECT_EQ(test_height, seatctrl_get_height(&ctx));

    EXPECT_EQ(0, seatctrl_close(&ctx));
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_position(&ctx)) << "Motor pos should be " << SEAT_CTRL_ERR << " after seatctrl_close()";
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_tilt(&ctx)) << "Motor pos should be " << SEAT_CTRL_ERR << " after seatctrl_close()";
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_height(&ctx)) << "Motor pos should be " << SEAT_CTRL_ERR << " after seatctrl_close()";
}



struct test_cb_t {
    int32_t received_pos; // what position was received in callback
    int32_t received_tilt; // what position was received in callback
    int32_t received_height; // what position was received in callback
};

static int pos_cb_calls = 0;
static int tilt_cb_calls = 0;
static int height_cb_calls = 0;

void motor_cb(SeatCtrlEvent event, int position, void* user_data)
{
    if (event == SeatCtrlEvent::MotorPos) {
        std::cout << "  >> motor_cb(" << position << ", " << user_data << ") #" << pos_cb_calls << std::endl;
        pos_cb_calls++;
        if (user_data) {
            test_cb_t* cb_data = (test_cb_t*)user_data;
            cb_data->received_pos = position;
        }
    }
    if (event == SeatCtrlEvent::MotorTilt) {
        std::cout << "  >> motor_tilt_cb(" << position << ", " << user_data << ") #" << tilt_cb_calls << std::endl;
        tilt_cb_calls++;
        if (user_data) {
            test_cb_t* cb_data = (test_cb_t*)user_data;
            cb_data->received_tilt = position;
        }
    }
    if (event == SeatCtrlEvent::MotorHeight) {
        std::cout << "  >> motor_cb(" << position << ", " << user_data << ") #" << height_cb_calls << std::endl;
        height_cb_calls++;
        if (user_data) {
            test_cb_t* cb_data = (test_cb_t*)user_data;
            cb_data->received_height = position;
        }
    }
}

/**
 * @brief Test seatctrl_set_event_callback().
 */
TEST_F(TestSeatCtrlApi, TestPosCallback) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    // check for SIGSEGV
    EXPECT_EQ(-EINVAL, seatctrl_set_event_callback(nullptr, nullptr, nullptr));

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    can_frame frame1;
    can_frame frame2;
    test_cb_t cb = { -1, -1, -1}; // invalid pos
    int test_pos = 42;
    int test_tilt = 50;
    int test_height = 35;

    ctx.running = true; // important! if not running callbacks are not called e.g. seatctrl_close concurrent call

    // set actual cb
    EXPECT_EQ(0, seatctrl_set_event_callback(&ctx, motor_cb, &cb));

    // initial secu stat change: callback not set yet, no data is received
    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame1, test_pos, MotorPosDirection::POS_OFF, PosLearningState::PosLearned, test_tilt, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame1));
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame2, test_height, MotorHeightDirection::HEIGHT_OFF, HeightLearningState::HeightLearned));
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame2));
    EXPECT_EQ(test_pos, cb.received_pos) << "Callback should have received: " << test_pos;
    EXPECT_EQ(test_tilt, cb.received_tilt) << "Callback should have received: " << test_tilt;
    EXPECT_EQ(test_height, cb.received_height) << "Callback should have received: " << test_height;


    // change motor pos
    test_pos++;
    test_tilt++;
    test_height++;
    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame1, test_pos, MotorPosDirection::POS_INC, PosLearningState::PosLearned, test_tilt, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame1));
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame2, test_height, MotorHeightDirection::HEIGHT_OFF, HeightLearningState::HeightLearned));
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame2));
    EXPECT_EQ(test_pos, cb.received_pos) << "Callback should have received: " << test_pos;
    EXPECT_EQ(test_tilt, cb.received_tilt) << "Callback should have received: " << test_tilt;
    EXPECT_EQ(test_height, cb.received_height) << "Callback should have received: " << test_height;

    // check with thea same pos, invalidate cb value
    cb.received_pos = -1;
    cb.received_tilt = -1;
    cb.received_height = -1;
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame1));
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame2));
    EXPECT_EQ(-1, cb.received_pos) << "Callback calles with the same value";
    EXPECT_EQ(-1, cb.received_tilt) << "Callback calles with the same value";
    EXPECT_EQ(-1, cb.received_height) << "Callback calles with the same value";

    // check invalid values
    EXPECT_EQ(0, seatctrl_set_event_callback(&ctx, motor_cb, nullptr));
    // change motor pos
    test_pos++;
    int old_calls = pos_cb_calls; // cache old cb counter
    cb.received_pos = -1;
    test_tilt++;
    int old_tilt_calls = tilt_cb_calls; // cache old cb counter
    cb.received_tilt = -1;
    test_height++;
    int old_height_calls = height_cb_calls; // cache old cb counter
    cb.received_height = -1;
    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame1, test_pos, MotorPosDirection::POS_INC, PosLearningState::PosLearned, test_tilt, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame1));
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame2, test_height, MotorHeightDirection::HEIGHT_OFF, HeightLearningState::HeightLearned));
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame2));
    EXPECT_EQ(-1, cb.received_pos) << "Callback value should not be updated";
    EXPECT_EQ(-1, cb.received_tilt) << "Callback value should not be updated";
    EXPECT_EQ(-1, cb.received_height) << "Callback value should not be updated";
    EXPECT_EQ(old_calls + 1, pos_cb_calls) << "Callback function should be called with null arg";
    EXPECT_EQ(old_tilt_calls + 1, tilt_cb_calls) << "Callback function should be called with null arg";
    EXPECT_EQ(old_height_calls + 1, height_cb_calls) << "Callback function should be called with null arg";

    // check invalid values
    EXPECT_EQ(0, seatctrl_set_event_callback(&ctx, nullptr, nullptr));
    // change motor pos
    test_pos++;
    old_calls = pos_cb_calls; // cache old cb counter
    cb.received_pos = -1;
    test_tilt++;
    old_tilt_calls = tilt_cb_calls; // cache old cb counter
    cb.received_tilt = -1;
    test_height++;
    old_height_calls = height_cb_calls; // cache old cb counter
    cb.received_height = -1;
    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame1, test_pos, MotorPosDirection::POS_INC, PosLearningState::PosLearned, test_tilt, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame1));
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame2, test_height, MotorHeightDirection::HEIGHT_OFF, HeightLearningState::HeightLearned));
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame2));
    EXPECT_EQ(-1, cb.received_pos) << "Callback value should not be updated";
    EXPECT_EQ(-1, cb.received_tilt) << "Callback value should not be updated";
    EXPECT_EQ(-1, cb.received_height) << "Callback value should not be updated";
    EXPECT_EQ(old_calls, pos_cb_calls) << "Callback function should be called with null arg";
    EXPECT_EQ(old_tilt_calls, tilt_cb_calls) << "Callback function should be called with null arg";
    EXPECT_EQ(old_height_calls, height_cb_calls) << "Callback function should be called with null arg";

    EXPECT_EQ(0, seatctrl_close(&ctx));
}


/**
 * @brief Tests internal seatctrl functions to improve code coverage..
 */
TEST_F(TestSeatCtrlApi, TestInternals) {

    // enable full dumps
    ::setenv("SC_RAW", "1", true);
    ::setenv("SC_CTL", "1", true);
    ::setenv("SC_STAT", "1", true);

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    can_frame frame;

    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 42, MotorPosDirection::POS_INV, PosLearningState::PosInvalid, 42, MotorTiltDirection::TILT_INV, TiltLearningState::TiltInvalid)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame, 42, MotorHeightDirection::HEIGHT_INV, HeightLearningState::HeightInvalid)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame));

    // check if invalid values are rejected (e.g. cangen frames).
    // Actually generated frames have enum values in range..
    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 255, 0xF, 0xE, 255, 0xF, 0xE)) << "Internal can generator failed!";
    frame.can_dlc = 1; // make it invalid
    EXPECT_EQ(-1, handle_secu2_stat(&ctx, &frame)) << "Invalid SECU_STAT values should return an error";
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame, 255, 0xF, 0xE)) << "Internal can generator failed!";
    frame.can_dlc = 1; // make it invalid
    EXPECT_EQ(-1, handle_secu1_stat(&ctx, &frame)) << "Invalid SECU_STAT values should return an error";

    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 99, MotorPosDirection::POS_INC, PosLearningState::PosNotLearned, 99, MotorTiltDirection::TILT_INC, TiltLearningState::TiltNotLearned)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));
    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 100, MotorPosDirection::POS_INC, PosLearningState::PosNotLearned, 100, MotorTiltDirection::TILT_INC, TiltLearningState::TiltNotLearned)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));

    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame, 99, MotorHeightDirection::HEIGHT_INC, HeightLearningState::HeightNotLearned)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame));
    EXPECT_EQ(0, GenerateSecu1StatFrame(&frame, 100, MotorHeightDirection::HEIGHT_DEC, HeightLearningState::HeightLearned)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu1_stat(&ctx, &frame));
    // just call it
    EXPECT_NO_THROW(print_can_raw(&frame, true));
}


/**
 * @brief Tests seatctrl_control_ecu12_loop() - Happy Path for increasing direction
 */
TEST_F(TestSeatCtrlApi, ControlLoopINC) {

    // TODO: Use TEST_P with parameters
    int initial_pos = 0;
    int initial_tilt = 0;
    int initial_height = 0;
    int target_pos = 90;
    int target_tilt = 90;
    int target_height = 90;

    SocketMock mock("/tmp/.test_seatctrl_api-ControlLoopINC.sock");
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    int sockfd = mock.getSocket();
    ASSERT_NE(SOCKET_INVALID, sockfd);

    // mock seatctrl_socket_open() entirely
    ctx.socket = sockfd;
    ctx.thread_id = 0xdeafbeef;
    ctx.running = true;

    // manipulate ctx :( to simulate motor movements..
    ctx.config.debug_ctl = false; // unless tests with verbose?
    ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF;
    ctx.motor_pos_learning_state = PosLearningState::PosLearned;
    ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF;
    ctx.motor_tilt_learning_state = TiltLearningState::TiltLearned;
    ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF;
    ctx.motor_height_learning_state = HeightLearningState::HeightLearned;
    ctx.motor_pos = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    ctx.motor_tilt = initial_tilt; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    ctx.motor_height = initial_height; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_pos, ctx.motor_pos) << "Must start from initial position: " << initial_pos;

    EXPECT_EQ(target_pos, ctx.desired_position);
    EXPECT_EQ(MotorPosDirection::POS_INC, ctx.desired_pos_direction);

    EXPECT_EQ(0, seatctrl_set_tilt(&ctx, target_tilt)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_tilt, ctx.motor_tilt) << "Must start from initial tilt: " << initial_pos;

    EXPECT_EQ(target_tilt, ctx.desired_tilt);
    EXPECT_EQ(MotorTiltDirection::TILT_INC, ctx.desired_tilt_direction);

    EXPECT_EQ(0, seatctrl_set_height(&ctx, target_height)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_height, ctx.motor_height) << "Must start from initial height: " << initial_pos;

    EXPECT_EQ(target_height, ctx.desired_height);
    EXPECT_EQ(MotorHeightDirection::HEIGHT_INC, ctx.desired_height_direction);

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    usleep(450 * 1000L); // simulate motor spin up time

    // do actual move(s)...
    ctx.pos_running = true;
    ctx.command_pos_ts = get_ts();
    for (auto pos = initial_pos-3; pos <= target_pos; pos++) {
        bool captured = false;
        if (pos == initial_pos-3) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_DEC;
            ctx.motor_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos-2) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_INC;
            ctx.motor_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos-1) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF;
            ctx.motor_pos = MOTOR_POS_INVALID;
        }
        else {
            ctx.motor_pos = pos;
            // simulate stop @ threshold
            if (pos == 85) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF; 
            } else {
                ctx.motor_pos_mov_state = MotorPosDirection::POS_INC;
            }
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Re-sending: SECU2_CMD_1") != std::string::npos)
                    << "Expected resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(10 * 1000L);  // 10ms
    }

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(target_pos, ctx.motor_pos) << "Motor should be at " << target_pos << "%";
    EXPECT_EQ(0, ctx.command_pos_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position) << "pending command should be finished!";;
    EXPECT_EQ(MotorPosDirection::POS_OFF, ctx.desired_pos_direction) << "pending command should be finished!";
    EXPECT_EQ(false, ctx.pos_running) << "pending command should be finished!";

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    ctx.tilt_running = true;
    ctx.command_tilt_ts = get_ts();
    for (auto tilt = initial_tilt-3; tilt <= target_pos; tilt++) {
        bool captured = false;
        if (tilt == initial_tilt-3) {
            ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_DEC;
            ctx.motor_tilt = MOTOR_POS_INVALID;
        } else
        if (tilt == initial_tilt-2) {
            ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_INC;
            ctx.motor_tilt = MOTOR_POS_INVALID;
        } else
        if (tilt == initial_tilt-1) {
            ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF;
            ctx.motor_tilt = MOTOR_POS_INVALID;
        }
        else {
            ctx.motor_tilt = tilt;
            // simulate stop @ threshold
            if (tilt == 85) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF; 
            } else {
                ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_INC; 
            }
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Re-sending: SECU2_CMD_1") != std::string::npos)
                    << "Expected resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(10 * 1000L);  // 10ms
    }

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(target_tilt, ctx.motor_tilt) << "Motor should be at " << target_tilt << "%";
    EXPECT_EQ(0, ctx.command_tilt_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_tilt) << "pending command should be finished!";;
    EXPECT_EQ(MotorTiltDirection::TILT_OFF, ctx.desired_tilt_direction) << "pending command should be finished!";
    EXPECT_EQ(false, ctx.tilt_running) << "pending command should be finished!";

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    ctx.height_running = true;
    ctx.command_height_ts = get_ts();
    for (auto height = initial_height-3; height <= target_height; height++) {
        bool captured = false;
        if (height == initial_height-3) {
            ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_DEC;
            ctx.motor_height = MOTOR_POS_INVALID;
        } else
        if (height == initial_height-2) {
            ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_INC;
            ctx.motor_height = MOTOR_POS_INVALID;
        } else
        if (height == initial_height-1) {
            ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF;
            ctx.motor_height = MOTOR_POS_INVALID;
        }
        else {
            ctx.motor_height = height;
            // simulate stop @ threshold
            if (height == 85) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF;
            } else {
                ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_INC;
            }
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Re-sending: SECU1_CMD_1") != std::string::npos)
                    << "Expected resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(10 * 1000L);  // 10ms
    }

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(target_height, ctx.motor_height) << "Motor should be at " << target_height << "%";
    EXPECT_EQ(0, ctx.command_height_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_height) << "pending command should be finished!";;
    EXPECT_EQ(MotorHeightDirection::HEIGHT_OFF, ctx.desired_height_direction) << "pending command should be finished!";
    EXPECT_EQ(false, ctx.height_running) << "pending command should be finished!";

    if (sockfd != SOCKET_INVALID) {
        ::close(sockfd);
    }
}

/**
 * @brief Tests seatctrl_control_ecu12_loop() - Happy Path for decreasing direction
 */
TEST_F(TestSeatCtrlApi, ControlLoopDEC) {

    // TODO: Use TEST_P with parameters
    int initial_pos = 100;
    int initial_tilt = 100;
    int initial_height = 100;
    int target_pos = 0;
    int target_tilt = 0;
    int target_height = 0;

    SocketMock mock("/tmp/.test_seatctrl_api-ControlLoopINC.sock");
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    int sockfd = mock.getSocket();
    ASSERT_NE(SOCKET_INVALID, sockfd);

    // mock seatctrl_socket_open() entirely
    ctx.socket = sockfd;
    ctx.thread_id = 0xdeafbeef;
    ctx.running = true;

    // manipulate ctx :( to simulate motor movements..
    ctx.config.debug_ctl = false; // unless tests with verbose?
    ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF;
    ctx.motor_pos_learning_state = PosLearningState::PosLearned;
    ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF;
    ctx.motor_tilt_learning_state = TiltLearningState::TiltLearned;
    ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF;
    ctx.motor_height_learning_state = HeightLearningState::HeightLearned;
    ctx.motor_pos = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    ctx.motor_tilt = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    ctx.motor_height = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_pos, ctx.motor_pos) << "Must start from initial position: " << initial_pos;

    EXPECT_EQ(target_pos, ctx.desired_position);
    EXPECT_EQ(MotorPosDirection::POS_DEC, ctx.desired_pos_direction);

    EXPECT_EQ(0, seatctrl_set_tilt(&ctx, target_tilt)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_tilt, ctx.motor_tilt) << "Must start from initial tilt: " << initial_pos;

    EXPECT_EQ(target_tilt, ctx.desired_tilt);
    EXPECT_EQ(MotorTiltDirection::TILT_DEC, ctx.desired_tilt_direction);

    EXPECT_EQ(0, seatctrl_set_height(&ctx, target_height)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_height, ctx.motor_height) << "Must start from initial height: " << initial_pos;

    EXPECT_EQ(target_height, ctx.desired_height);
    EXPECT_EQ(MotorHeightDirection::HEIGHT_DEC, ctx.desired_height_direction);

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    usleep(450 * 1000L); // simulate motor spin up time

    // do actual move(s)...
    ctx.pos_running = true;
    ctx.command_pos_ts = get_ts();
    for (auto pos = initial_pos+3; pos >= target_pos; pos--) {
        bool captured = false;
        if (pos == initial_pos+3) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_DEC;
            ctx.motor_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos+2) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_INC;
            ctx.motor_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos+1) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF;
            ctx.motor_pos = MOTOR_POS_INVALID;
        }
        else {
            ctx.motor_pos = pos;
            // simulate stop @ threshold
            if (pos == 14) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF; 
            } else {
                ctx.motor_pos_mov_state = MotorPosDirection::POS_DEC; 
            }
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Re-sending: SECU2_CMD_1") != std::string::npos)
                    << "Expected resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(10 * 1000L);  // 10ms
    }

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(target_pos, ctx.motor_pos) << "Motor should be at " << target_pos << "%";
    EXPECT_EQ(0, ctx.command_pos_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position) << "pending command should be finished!";;
    EXPECT_EQ(MotorPosDirection::POS_OFF, ctx.desired_pos_direction) << "pending command should be finished!";
    EXPECT_EQ(false, ctx.pos_running) << "pending command should be finished!";

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    ctx.tilt_running = true;
    ctx.command_tilt_ts = get_ts();
    for (auto tilt = initial_tilt+3; tilt >= target_tilt; tilt--) {
        bool captured = false;
        if (tilt == initial_tilt+3) {
            ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_DEC;
            ctx.motor_tilt = MOTOR_POS_INVALID;
        } else
        if (tilt == initial_tilt+2) {
            ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_INC;
            ctx.motor_tilt = MOTOR_POS_INVALID;
        } else
        if (tilt == initial_tilt+1) {
            ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF;
            ctx.motor_tilt = MOTOR_POS_INVALID;
        }
        else {
            ctx.motor_tilt = tilt;
            // simulate stop @ threshold
            if (tilt == 14) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF;
            } else {
                ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_DEC;
            }
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Re-sending: SECU2_CMD_1") != std::string::npos)
                    << "Expected resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(10 * 1000L);  // 10ms
    }

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(target_tilt, ctx.motor_tilt) << "Motor should be at " << target_tilt << "%";
    EXPECT_EQ(0, ctx.command_tilt_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_tilt) << "pending command should be finished!";;
    EXPECT_EQ(MotorTiltDirection::TILT_OFF, ctx.desired_tilt_direction) << "pending command should be finished!";
    EXPECT_EQ(false, ctx.tilt_running) << "pending command should be finished!";

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    ctx.height_running = true;
    ctx.command_height_ts = get_ts();
    for (auto height = initial_height+3; height >= target_height; height--) {
        bool captured = false;
        if (height == initial_height+3) {
            ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_DEC;
            ctx.motor_height = MOTOR_POS_INVALID;
        } else
        if (height == initial_height+2) {
            ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_INC;
            ctx.motor_height = MOTOR_POS_INVALID;
        } else
        if (height == initial_height+1) {
            ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF;
            ctx.motor_height = MOTOR_POS_INVALID;
        }
        else {
            ctx.motor_height = height;
            // simulate stop @ threshold
            if (height == 14) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF; 
            } else {
                ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_DEC; 
            }
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Re-sending: SECU1_CMD_1") != std::string::npos)
                    << "Expected resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(10 * 1000L);  // 10ms
    }

    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(target_height, ctx.motor_height) << "Motor should be at " << target_height << "%";
    EXPECT_EQ(0, ctx.command_height_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_height) << "pending command should be finished!";;
    EXPECT_EQ(MotorHeightDirection::HEIGHT_OFF, ctx.desired_height_direction) << "pending command should be finished!";
    EXPECT_EQ(false, ctx.height_running) << "pending command should be finished!";

    if (sockfd != SOCKET_INVALID) {
        ::close(sockfd);
    }
}


/**
 * @brief Tests seatctrl_control_ecu12_loop() - Happy Path for increasing direction
 */
TEST_F(TestSeatCtrlApi, ControlLoopTimeout) {

    // TODO: Use TEST_P with parameters
    int initial_pos = 20;
    int target_pos = 90;
    int timeout = 50; // 1ms should always timeout


    SocketMock mock("/tmp/.test_seatctrl_api-ControlLoopTimeout.sock");
    EXPECT_EQ(0, seatctrl_default_config(&config));
    config.command_pos_timeout = timeout; // override to 50ms

    EXPECT_EQ(-EINVAL, seatctrl_set_position(nullptr, 42)); // Test with invalid context
    EXPECT_EQ(-EINVAL, seatctrl_set_position(&ctx, 42)); // Test with unintialized context

    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    int sockfd = mock.getSocket();
    ASSERT_NE(SOCKET_INVALID, sockfd);

    // mock seatctrl_socket_open() entirely
    ctx.socket = sockfd;
    ctx.thread_id = 0xdeadbeef;
    ctx.running = true;

    // manipulate ctx :( to simulate motor movements..
    ctx.config.debug_ctl = false; // unless tests with verbose?
    ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF;
    ctx.motor_pos_learning_state = PosLearningState::PosLearned;
    ctx.motor_tilt_mov_state = MotorTiltDirection::TILT_OFF;
    ctx.motor_tilt_learning_state = TiltLearningState::TiltLearned;
    ctx.motor_height_mov_state = MotorHeightDirection::HEIGHT_OFF;
    ctx.motor_height_learning_state = HeightLearningState::HeightLearned;
    ctx.motor_pos = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_pos, ctx.motor_pos) << "Must start from initial position: " << initial_pos;

    EXPECT_EQ(target_pos, ctx.desired_position);
    EXPECT_EQ(MotorPosDirection::POS_INC, ctx.desired_pos_direction);

    // do actual move(s)...
    ctx.pos_running = true;
    ctx.command_pos_ts = get_ts();
    for (auto pos = initial_pos; pos <= target_pos; pos++) {
        if (pos == 85) {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_OFF; 
        } else {
            ctx.motor_pos_mov_state = MotorPosDirection::POS_INC; 
        }
        EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
        usleep(1 * 1000L);  // 1ms
    }
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));


    EXPECT_NE(target_pos, ctx.motor_pos) << "Motor should be at different position than: " << target_pos << "%";
    EXPECT_EQ(0, ctx.command_pos_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position) << "pending command should be finished!";;
    EXPECT_EQ(MotorPosDirection::POS_OFF, ctx.desired_pos_direction) << "pending command should be finished!";

    if (sockfd != SOCKET_INVALID) {
        ::close(sockfd);
    }
}

/**
 * @brief Tests Not-learned state detection..
 */
TEST_F(TestSeatCtrlApi, TestNotLearnedMode) {
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    // state is needed to call seatctrl_control_ecu12_loop()
    ctx.running = true;
    // enable full dumps
    ctx.config.debug_raw = true;
    ctx.config.debug_stats = true;
    ctx.config.debug_ctl = true;

    can_frame frame;
    std::string output;

    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, MOTOR_POS_INVALID, MotorPosDirection::POS_OFF, PosLearningState::PosNotLearned, MOTOR_POS_INVALID, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltNotLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));

    testing::internal::CaptureStdout();
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
    output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("ECU in not-learned state") != std::string::npos) << "Expected not-learned state warning, got: " << output;


    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 0, MotorPosDirection::POS_OFF, PosLearningState::PosLearned, MOTOR_POS_INVALID, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltNotLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
    output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.find("ECU in not-learned state") != std::string::npos) << "Duplicate not-learned state warning: " << output;

    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 0, MotorPosDirection::POS_OFF, PosLearningState::PosNotLearned, MOTOR_POS_INVALID, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltNotLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    EXPECT_EQ(0, GenerateSecu2StatFrame(&frame, 1, MotorPosDirection::POS_OFF, PosLearningState::PosLearned, MOTOR_POS_INVALID, MotorTiltDirection::TILT_OFF, TiltLearningState::TiltLearned));
    EXPECT_EQ(0, handle_secu2_stat(&ctx, &frame));
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));

    testing::internal::CaptureStdout();
    EXPECT_EQ(0, seatctrl_control_ecu12_loop(&ctx));
    output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("ECU changed to: learned state") == std::string::npos) << "Duplicate learned state: " << output;

    EXPECT_EQ(0, seatctrl_close(&ctx));
}

}  // namespace test
}  // namespace sdv
