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
extern int seatctrl_controll_loop(seatctrl_context_t *ctx);
/**
 * @brief
 *
 */
extern int handle_secu_stat(seatctrl_context_t *ctx, const struct can_frame *frame);
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
        ::unsetenv("SC_TIMEOUT");
        ::unsetenv("SC_RPM");
        ::unsetenv("SC_RAW");
        ::unsetenv("SC_VERBOSE");
        ::unsetenv("SC_STAT");
        ::unsetenv("SC_CTL");
    }

    /**
     * @brief Generates SECU_STAT can frame with specified position, movment state and learning state for motor1
     *
     * @param frame Pointer to can_frame
     * @param motor_pos desired motor pos
     * @param mov_state desired movement state
     * @param learn_state desired learning state
     */
    int GenerateSecuStatFrame(struct can_frame *frame, int motor_pos, int mov_state, int learn_state) {
        if (!frame) return -1;
        ::memset(frame, 0, sizeof(can_frame));

        CAN_secu1_stat_t stat;
        ::memset(&stat, 0, sizeof(CAN_secu1_stat_t));
        stat.motor1_pos = motor_pos;
        stat.motor1_mov_state = mov_state;
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
    EXPECT_EQ(DEFAULT_RPM, config.motor_rpm);
    EXPECT_EQ(DEFAULT_OPERATION_TIMEOUT, config.command_timeout);
}

/**
 * @brief Test seatctrl_default_config() overide from environment variables
 */
TEST_F(TestSeatCtrlApi, TestConfigEnv) {
    // NOTE: ctx and config memory is invalidated in SetUp()

    ::setenv("SC_CAN", "vcan0", true);
    ::setenv("SC_TIMEOUT", "12345", true);
    ::setenv("SC_RPM", "99", true);  // invalid value! should reset to default

    ::setenv("SC_RAW", "INVALID", true);
    ::setenv("SC_VERBOSE", "1", true);
    ::setenv("SC_STAT", "1", true);
    ::setenv("SC_CTL", "0", true);

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_STREQ("vcan0", config.can_device);
    EXPECT_EQ(99, config.motor_rpm);
    EXPECT_EQ(12345, config.command_timeout);

    EXPECT_EQ(false, config.debug_raw);  // invalid integer=0
    EXPECT_EQ(true, config.debug_verbose);
    EXPECT_EQ(true, config.debug_stats);
    EXPECT_EQ(false, config.debug_ctl);

    // check invalid range for RMP [0..254]
    ::setenv("SC_RPM", "255", true);                 // invalid value! should reset to default
    EXPECT_EQ(-EINVAL, seatctrl_default_config(&config));  // should fail
    EXPECT_EQ(DEFAULT_RPM, config.motor_rpm);          // also sets default rpm
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
    EXPECT_EQ(config.motor_rpm, ctx.config.motor_rpm);
    EXPECT_EQ(config.command_timeout, ctx.config.command_timeout);
    EXPECT_EQ(config.debug_ctl, ctx.config.debug_ctl);
    EXPECT_EQ(config.debug_raw, ctx.config.debug_raw);
    EXPECT_EQ(config.debug_stats, ctx.config.debug_stats);
    EXPECT_EQ(config.debug_verbose, ctx.config.debug_verbose);

    // check if current operation is reset
    EXPECT_EQ(ctx.command_ts, 0);
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);

    // check if initial stats are invalidated
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.motor1_pos);
    EXPECT_EQ(MotorDirection::INV, ctx.motor1_mov_state);
    EXPECT_EQ(LearningState::Invalid, ctx.motor1_learning_state);

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

    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, test_pos, MotorDirection::INV, LearningState::Invalid)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));

    //ctx.motor1_pos = test_pos;
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_position(&ctx)) << "Motor pos should be: " << SEAT_CTRL_ERR << " if CTL thread is not running";
    ctx.running = true;
    EXPECT_EQ(test_pos, seatctrl_get_position(&ctx));

    EXPECT_EQ(0, seatctrl_close(&ctx));
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_position(&ctx)) << "Motor pos should be " << SEAT_CTRL_ERR << " after seatctrl_close()";
}



struct test_pos_cb_t {
    int32_t received_pos; // what position was received in callback
};

static int pos_cb_calls = 0;

void motor_pos_cb(SeatCtrlEvent event, int position, void* user_data)
{
    if (event == SeatCtrlEvent::Motor1Pos) {
        std::cout << "  >> motor_pos_cb(" << position << ", " << user_data << ") #" << pos_cb_calls << std::endl;
        pos_cb_calls++;
        if (user_data) {
            test_pos_cb_t* cb_data = (test_pos_cb_t*)user_data;
            cb_data->received_pos = position;
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

    can_frame frame;
    test_pos_cb_t cb = { -1 }; // invalid pos
    int test_pos = 42;

    ctx.running = true; // important! if not running callbacks are not called e.g. seatctrl_close concurrent call

    // set actual cb
    EXPECT_EQ(0, seatctrl_set_event_callback(&ctx, motor_pos_cb, &cb));

    // initial secu stat change: callback not set yet, no data is received
    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, test_pos, MotorDirection::OFF, LearningState::Learned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(test_pos, cb.received_pos) << "Callback should have received: " << test_pos;


    // change motor pos
    test_pos++;
    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, test_pos, MotorDirection::INC, LearningState::Learned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(test_pos, cb.received_pos) << "Callback should have received: " << test_pos;

    // check with thea same pos, invalidate cb value
    cb.received_pos = -1;
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(-1, cb.received_pos) << "Callback calles with the same value";

    // check invalid values
    EXPECT_EQ(0, seatctrl_set_event_callback(&ctx, motor_pos_cb, nullptr));
    // change motor pos
    test_pos++;
    int old_calls = pos_cb_calls; // cache old cb counter
    cb.received_pos = -1;
    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, test_pos, MotorDirection::INC, LearningState::Learned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(-1, cb.received_pos) << "Callback value should not be updated";
    EXPECT_EQ(old_calls + 1, pos_cb_calls) << "Callback function should be called with null arg";

    // check invalid values
    EXPECT_EQ(0, seatctrl_set_event_callback(&ctx, nullptr, nullptr));
    // change motor pos
    test_pos++;
    old_calls = pos_cb_calls; // cache old cb counter
    cb.received_pos = -1;
    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, test_pos, MotorDirection::OFF, LearningState::Learned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(-1, cb.received_pos) << "Callback value should not be updated";
    EXPECT_EQ(old_calls, pos_cb_calls) << "Callback function should not be called";

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

    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 42, MotorDirection::INV, LearningState::Invalid)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));

    // check if invalid values are rejected (e.g. cangen frames).
    // Actually generated frames have enum values in range..
    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 255, 0xF, 0xE)) << "Internal can generator failed!";
    frame.can_dlc = 1; // make it invalid
    EXPECT_EQ(-1, handle_secu_stat(&ctx, &frame)) << "Invalid SECU_STAT values should return an error";

    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 99, MotorDirection::INC, LearningState::NotLearned)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 100, MotorDirection::DEC, LearningState::Learned)) << "Internal can generator failed!";
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    // just call it
    EXPECT_NO_THROW(print_can_raw(&frame, true));
}


/**
 * @brief Tests seatctrl_controll_loop() - Happy Path for increasing direction
 */
TEST_F(TestSeatCtrlApi, ControlLoopINC) {

    // TODO: Use TEST_P with parameters
    int initial_pos = 0;
    int target_pos = 100;

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
    ctx.motor1_mov_state = MotorDirection::OFF;
    ctx.motor1_learning_state = LearningState::Learned;
    ctx.motor1_pos = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    auto now_ts = get_ts();
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_pos, ctx.motor1_pos) << "Must start from initial position: " << initial_pos;

    EXPECT_GT(ctx.command_ts, now_ts);
    EXPECT_EQ(target_pos, ctx.desired_position);
    EXPECT_EQ(MotorDirection::INC, ctx.desired_direction);

    usleep(450 * 1000L); // simulate motor spin up time

    // do actual move(s)...
    for (auto pos = initial_pos-3; pos <= target_pos; pos++) {
        bool captured = false;
        if (pos == initial_pos-3) {
            ctx.motor1_mov_state = MotorDirection::DEC;
            ctx.motor1_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos-2) {
            ctx.motor1_mov_state = MotorDirection::INC;
            ctx.motor1_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos-1) {
            ctx.motor1_mov_state = MotorDirection::OFF;
            ctx.motor1_pos = MOTOR_POS_INVALID;
            ctx.motor1_learning_state = LearningState::NotLearned;
        }
        else {
            ctx.motor1_pos = pos;
            ctx.motor1_learning_state = LearningState::Learned;
            // simulate stop @ threshold
            if (pos == 85) {
                testing::internal::CaptureStdout();
                captured = true;
                ctx.motor1_mov_state = MotorDirection::OFF; // == desired direction
            } else {
                ctx.motor1_mov_state = MotorDirection::INC; // == desired direction
            }
        }
        EXPECT_EQ(0, seatctrl_controll_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Sending MotorOff command") != std::string::npos &&
                        output.find("Re-sending: SECU1_CMD_1") != std::string::npos)
                    << "Expected motor off + resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }
        usleep(1 * 1000L);  // 1ms
    }

    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    EXPECT_EQ(target_pos, ctx.motor1_pos) << "Motor should be at " << target_pos << "%";
    EXPECT_EQ(0, ctx.command_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position) << "pending command should be finished!";;
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction) << "pending command should be finished!";
    if (sockfd != SOCKET_INVALID) {
        ::close(sockfd);
    }
}

/**
 * @brief Tests seatctrl_controll_loop() - Happy Path for decreasing direction
 */
TEST_F(TestSeatCtrlApi, ControlLoopDEC) {

    // TODO: Use TEST_P with parameters
    int initial_pos = 100;
    int target_pos = 0;

    SocketMock mock("/tmp/.test_seatctrl_api-ControlLoopDEC.sock");
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    int sockfd = mock.getSocket();
    //int sockfd = _alloc_unix_socket("/tmp/.test_seat_controller.sock");
    ASSERT_NE(SOCKET_INVALID, sockfd);

    // mock seatctrl_socket_open() entirely
    ctx.socket = sockfd;
    ctx.thread_id = 0xdeadbeef;
    ctx.running = true;

    // manipulate ctx :( to simulate motor movements..
    ctx.config.debug_ctl = false;
    ctx.config.command_timeout = 60000;
    ctx.motor1_mov_state = MotorDirection::OFF;
    ctx.motor1_learning_state = LearningState::Learned;
    ctx.motor1_pos = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    // well, socket write fails
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_pos, ctx.motor1_pos) << "Must start from initial position: " << initial_pos;

    EXPECT_NE(ctx.command_ts, 0);
    EXPECT_EQ(target_pos, ctx.desired_position);
    EXPECT_EQ(MotorDirection::DEC, ctx.desired_direction);

    usleep(450 * 1000L); // simulate motor spin up time

    // do actual move(s)...
    for (auto pos = initial_pos + 3; pos >= target_pos; pos--) {
        bool captured = false;
        // throw in some random states
        if (pos == initial_pos + 3) {
            ctx.motor1_mov_state = MotorDirection::DEC;
            ctx.motor1_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos + 2) {
            ctx.motor1_mov_state = MotorDirection::INC;
            ctx.motor1_pos = MOTOR_POS_INVALID;
        } else
        if (pos == initial_pos + 1) {
            ctx.motor1_mov_state = MotorDirection::OFF;
            ctx.motor1_pos = MOTOR_POS_INVALID;
            ctx.motor1_learning_state = LearningState::NotLearned;
        }
        else {
            ctx.motor1_pos = pos;
            ctx.motor1_learning_state = LearningState::Learned;
            // simulate stop @ threshold
            if (pos == 14) {
                ctx.motor1_mov_state = MotorDirection::OFF; // == desired direction
                // capture stdout to check for stop / re-stat cmd1
                testing::internal::CaptureStdout();
                captured = true;
            } else {
                ctx.motor1_mov_state = MotorDirection::DEC; // == desired direction
            }
        }
        EXPECT_EQ(0, seatctrl_controll_loop(&ctx));
        if (captured) {
            std::string output = testing::internal::GetCapturedStdout();
            std::cout << output << std::endl;
            EXPECT_TRUE(output.find("Sending MotorOff command") != std::string::npos &&
                        output.find("Re-sending: SECU1_CMD_1") != std::string::npos)
                    << "Expected motor off + resend command on auto stop:\n---\n" << output << "\n---";
            captured = false;
        }

        usleep(1 * 1000L);  // 1ms
    }

    // command should be handled already
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    EXPECT_EQ(target_pos, ctx.motor1_pos) << "Motor should be at " << target_pos << "%";
    EXPECT_EQ(0, ctx.command_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position) << "pending command should be finished!";;
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction) << "pending command should be finished!";

    if (sockfd != SOCKET_INVALID) {
        ::close(sockfd);
    }
}


/**
 * @brief Tests seatctrl_controll_loop() - Happy Path for increasing direction
 */
TEST_F(TestSeatCtrlApi, ControlLoopTimeout) {

    // TODO: Use TEST_P with parameters
    int initial_pos = 20;
    int target_pos = 90;
    int timeout = 50; // 1ms should always timeout


    SocketMock mock("/tmp/.test_seatctrl_api-ControlLoopTimeout.sock");
    EXPECT_EQ(0, seatctrl_default_config(&config));
    config.command_timeout = timeout; // override to 50ms

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
    ctx.motor1_mov_state = MotorDirection::OFF;
    ctx.motor1_learning_state = LearningState::Learned;
    ctx.motor1_pos = initial_pos; // can't start with MOTOR_POS_INVALID, as it needs another thread to change it
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    auto now_ts = get_ts();
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos)) << "May fail if socket mock is not connected..";
    EXPECT_EQ(initial_pos, ctx.motor1_pos) << "Must start from initial position: " << initial_pos;

    EXPECT_GT(ctx.command_ts, now_ts);
    EXPECT_EQ(target_pos, ctx.desired_position);
    EXPECT_EQ(MotorDirection::INC, ctx.desired_direction);

    // do actual move(s)...
    for (auto pos = initial_pos; pos <= target_pos; pos++) {
        if (pos == 85) {
            ctx.motor1_mov_state = MotorDirection::OFF; // == desired direction
        } else {
            ctx.motor1_mov_state = MotorDirection::INC; // == desired direction
        }
        EXPECT_EQ(0, seatctrl_controll_loop(&ctx));
        usleep(1 * 1000L);  // 1ms
    }
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));


    EXPECT_NE(target_pos, ctx.motor1_pos) << "Motor should be at dofferent position than: " << target_pos << "%";
    EXPECT_EQ(0, ctx.command_ts) << "pending command should be finished!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position) << "pending command should be finished!";;
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction) << "pending command should be finished!";

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

    // state is needed to call seatctrl_controll_loop()
    ctx.running = true;
    // enable full dumps
    ctx.config.debug_raw = true;
    ctx.config.debug_stats = true;
    ctx.config.debug_ctl = true;

    can_frame frame;
    std::string output;

    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, MOTOR_POS_INVALID, MotorDirection::OFF, LearningState::NotLearned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));

    testing::internal::CaptureStdout();
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));
    output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("ECU in not-learned state") != std::string::npos) << "Expected not-learned state warning, got: " << output;


    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 0, MotorDirection::OFF, LearningState::Learned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    testing::internal::CaptureStdout();
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));
    output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.find("ECU in not-learned state") != std::string::npos) << "Dublicate not-learned state warning: " << output;

    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 0, MotorDirection::OFF, LearningState::NotLearned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    EXPECT_EQ(0, GenerateSecuStatFrame(&frame, 1, MotorDirection::OFF, LearningState::Learned));
    EXPECT_EQ(0, handle_secu_stat(&ctx, &frame));
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));

    testing::internal::CaptureStdout();
    EXPECT_EQ(0, seatctrl_controll_loop(&ctx));
    output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("ECU changed to: learned state") == std::string::npos) << "Dublicate learned state: " << output;

    EXPECT_EQ(0, seatctrl_close(&ctx));
}

}  // namespace test
}  // namespace sdv
