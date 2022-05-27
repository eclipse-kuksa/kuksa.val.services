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

#include <mutex>
#include <stdlib.h>
#include <sys/un.h>
#include <time.h>
#include <thread>

#include "seat_controller.h"

#include "gtest/gtest.h"

// for increasing cansim coverage, expecting to have cansim library linked!
extern "C" void sim_init(void);
extern "C" void sim_fini(void);
extern "C" void* sim_context();

namespace sdv {
namespace test {

/**
 * @brief Integration test for seat_controller. Links to cansim library that mocks
 * SocketCAN libc calls for the process. It also runs SeatAdjuster Engine simulation,
 * thatr can be controlled via different "SAE_*" environment variables).
 */
class SeatCtrlIntegrationTest : public ::testing::Test {

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
        ctx.socket = SOCKET_INVALID;
        ctx.running = false;
        ctx.config.can_device = NULL;
        ctx.thread_id = (pthread_t)NULL;
        // SAE debug props
        //::setenv("CANSIM_DEBUG", "1", true);
        ::setenv("SAE_DEBUG", "1", true);
        ::setenv("SAE_STOP", "1", true);
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

  protected:
    seatctrl_config_t config;
    seatctrl_context_t ctx;
    std::mutex mutex;
};


/**
 * @brief Test setting default positions from V-Apps (30% and 50%)
 */
TEST_F(SeatCtrlIntegrationTest, TestDemoPositions) {
    mutex.lock(); // guard SocketCanMock.instance()
    std::cout << "[TestDemoPositions] Started ..." << std::endl;

    // time in ms to wait for seatctrl_set_position()
    const int wait_timeout = 10000;

    int wait_time;
    int target_pos;

    ::setenv("SC_CAN", "cansim-TestDemoPositions", true);
    ::setenv("SC_TIMEOUT", std::to_string(wait_timeout).c_str(), true);
    ::setenv("SC_RPM", "80", true); // keep it within +/- 1 increments
    ::setenv("SC_CTL", "1", true);
    ::setenv("SC_STAT", "0", true);

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    ::setenv("SAE_POS", "0", true); // env. vars are re-initialized on socketcan socket() call
    EXPECT_EQ(0, seatctrl_open(&ctx)); // start reading from mocked socket

    target_pos = 50;
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    printf("### Waiting %d sec for reaching desired position ...\n", wait_timeout);
    for (wait_time = 0; wait_time <= wait_timeout; wait_time++) {
        ::usleep(1000L);  // wait 1ms
        if (seatctrl_get_position(&ctx) >= target_pos) break;
    }
    ::usleep(100 * 1000L); // give ctl time to read next can frame to update motor1_mov_state

    EXPECT_EQ(target_pos, seatctrl_get_position(&ctx)) << "Expected position " << target_pos << " not reached!";
    EXPECT_LE(wait_time, wait_timeout) << "Set timed out after " << wait_time << " ms!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);
    EXPECT_EQ(MotorDirection::OFF, ctx.motor1_mov_state);

    target_pos = 30;
    // test threshold stop (14):
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    for (wait_time = 0; wait_time <= wait_timeout; wait_time++) {
        ::usleep(1000L);  // wait 1ms
        if (seatctrl_get_position(&ctx) == target_pos) break;
    }
    ::usleep(100 * 1000L); // give ctl time to read next can frame to update motor1_mov_state

    EXPECT_EQ(target_pos, seatctrl_get_position(&ctx)) << "Expected position " << target_pos << " not reached!";
    EXPECT_LE(wait_time, wait_timeout) << "Set timed out after " << wait_time << " ms!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);
    EXPECT_EQ(MotorDirection::OFF, ctx.motor1_mov_state);

    // test setting same position
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    EXPECT_EQ(target_pos, seatctrl_get_position(&ctx)) << "Expected position " << target_pos << " not reached!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);
    EXPECT_EQ(MotorDirection::OFF, ctx.motor1_mov_state);

    ::usleep(100 * 1000L);

    EXPECT_EQ(0, seatctrl_close(&ctx));
    mutex.unlock();
}

TEST_F(SeatCtrlIntegrationTest, TestFastMove) {

    mutex.lock(); // guard SocketCanMock.instance()
    std::cout << "[TestFastMove] Started ..." << std::endl;

    // time in ms to wait for seatctrl_set_position()
    const int wait_timeout = 5000;

    int wait_time;
    int target_pos;

    // sdv::test::mock::SocketCanMock::instance().StartMocking(-1, can_read_cb, can_write_cb);
    ::setenv("SC_TIMEOUT", std::to_string(wait_timeout).c_str(), true);
    ::setenv("SC_RPM", "254", true);
    ::setenv("SC_CTL", "0", true);
    ::setenv("SC_STAT", "0", true);
    ::setenv("SC_CAN", "cansim-TestFastMove", true);
    ::setenv("SAE_STOP", "0", true); // don't stop on tresholds
    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    // put a valid initial position (not invalid)
    // ctx.motor1_mov_state = MotorDirection::OFF;
    // initial position without sim should be invalid
    EXPECT_EQ(SEAT_CTRL_ERR, seatctrl_get_position(&ctx));

    //_sim_active = true;
    ::setenv("SAE_POS", "0", true); // env. vars are re-initialized on socketcan socket() call
    EXPECT_EQ(0, seatctrl_open(&ctx)); // start reading from mocked socket

    target_pos = 99;
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    printf("### Waiting %d sec for reaching desired position ...\n", wait_timeout);
    for (wait_time = 0; wait_time <= wait_timeout; wait_time++) {
        ::usleep(1000L);  // wait 1ms
        if (seatctrl_get_position(&ctx) >= target_pos) break;
    }
    ::usleep(100 * 1000L); // give ctl time to read next can frame to update motor1_mov_state

    EXPECT_LE(target_pos, seatctrl_get_position(&ctx)) << "Expected position " << target_pos << " not reached!";
    EXPECT_LE(wait_time, wait_timeout) << "Set timed out after " << wait_time << " ms!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);
    EXPECT_EQ(MotorDirection::OFF, ctx.motor1_mov_state);

    target_pos = 1;
    // test threshold stop (14) and overshooting position
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    for (wait_time = 0; wait_time <= wait_timeout; wait_time++) {
        ::usleep(1000L);  // wait 1ms
        if (seatctrl_get_position(&ctx) <= target_pos) break;
    }
    ::usleep(100 * 1000L); // give ctl time to read next can frame to update motor1_mov_state

    EXPECT_GE(target_pos, seatctrl_get_position(&ctx)) << "Expected position " << target_pos << " not reached!";
    EXPECT_LE(wait_time, wait_timeout) << "Set timed out after " << wait_time << " ms!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);
    EXPECT_EQ(MotorDirection::OFF, ctx.motor1_mov_state);

    EXPECT_EQ(0, seatctrl_close(&ctx));
    mutex.unlock();
}

TEST_F(SeatCtrlIntegrationTest, TestMoveTimeout) {
    mutex.lock(); // guard SocketCanMock.instance()
    std::cout << "[TestMoveTimeout] Started ..." << std::endl;

    // time in ms to wait for seatctrl_set_position()
    const int wait_timeout = 2000;
    int wait_time;
    int target_pos;

    ::setenv("SC_CAN", "cansim-TestMoveTimeout", true);
    ::setenv("SC_TIMEOUT", "500", true); // 500ms
    ::setenv("SC_RPM", "30", true);
    ::setenv("SC_CTL", "0", true);
    ::setenv("SC_STAT", "0", true);


    EXPECT_EQ(0, seatctrl_default_config(&config));
    config.debug_stats = false;
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));

    ::setenv("SAE_POS", "0", true); // env. vars are re-initialized on socketcan socket() call
    EXPECT_EQ(0, seatctrl_open(&ctx)); // start reading from mocked socket

    target_pos = 85;
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    for (wait_time = 0; wait_time <= wait_timeout; wait_time++) {
        if (seatctrl_get_position(&ctx) >= target_pos) break;
        ::usleep(1000);  // wait 1ms
    }
    ::usleep(100 * 1000L); // give ctl time to read next can frame to update motor1_mov_state
    EXPECT_NE(target_pos, seatctrl_get_position(&ctx)) << "Expected position " << target_pos << " should not be reached!";
    EXPECT_EQ(MOTOR_POS_INVALID, ctx.desired_position);
    EXPECT_EQ(MotorDirection::OFF, ctx.desired_direction);
    EXPECT_EQ(MotorDirection::OFF, ctx.motor1_mov_state);

    EXPECT_EQ(0, seatctrl_close(&ctx));
    mutex.unlock();
}


#include <arpa/inet.h>

TEST_F(SeatCtrlIntegrationTest, CansimTest) {
    int rc;
    int errno__;
    uint8_t buf[16];
    struct ifreq ifr;
    struct sockaddr_in sa;

    mutex.lock();
    std::cout << "[CansimTest] Started ..." << std::endl;

    ::unsetenv("CANSIM_LOG");
    ::setenv("CANSIM_DEBUG", "1", true);
    ::setenv("CANSIM_VERBOSE", "1", true);
    ::sim_fini();
    ::sim_init();

    ::sim_context();

    // open mocked socket
    int canfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    EXPECT_NE(-1, canfd) << "SocketCAN socket() should succeed";

    // create dummy socket
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_NE(-1, fd) << "socket(AF_INET) should succeed";

    // bind to localhost:65535
    ::memset((char *)&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons( 65535 );
    rc = ::bind(fd, (struct sockaddr*) &sa, sizeof(sa));
    EXPECT_EQ(0, rc);

    // bind failure on wrong size canfd
    rc = ::bind(canfd, (struct sockaddr*) &sa, sizeof(sa)-1);
    errno__ = errno;
    EXPECT_EQ(-1, rc);
    EXPECT_EQ(EINVAL, errno__) << "errno should be EINVAL";

    // set 1 sec timeout
    timeval tv = { 1, 0 };
    rc = ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    EXPECT_EQ(0, rc);
    // set 1 sec timeout on canfd
    rc = ::setsockopt(canfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    EXPECT_EQ(0, rc);

    // check if ioctl(SIOCGIFINDEX) does not fail on any fd...
    ::memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "lo", IFNAMSIZ-1); // max 16!
    rc = ioctl(fd, SIOCGIFINDEX, &ifr);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(0, ifr.ifr_ifindex) << "Expected 'lo' index 0";

    ::memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "canXXX", IFNAMSIZ-1);
    rc = ioctl(fd, SIOCGIFINDEX, &ifr);
    EXPECT_EQ(0, rc);
    EXPECT_GE(ifr.ifr_ifindex, 0);

    // same on canfd
    ::memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "canXXX", IFNAMSIZ-1);
    rc = ioctl(canfd, SIOCGIFINDEX, &ifr);
    EXPECT_EQ(0, rc);
    EXPECT_GE(ifr.ifr_ifindex, 0);

    // check some other ioctl request..
    ::memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "lo", IFNAMSIZ-1);
    rc = ioctl(fd, SIOCGIFMTU, &ifr);
    EXPECT_EQ(0, rc);
    EXPECT_GT(ifr.ifr_mtu, 0) << "Unexpected localhost MTU"; // usually expect 0xFFFF for localhost mtu

    ::memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, "vcanXXX", IFNAMSIZ-1);
    rc = ioctl(canfd, SIOCGIFMTU, &ifr);
    EXPECT_EQ(0, rc) << "Unhandled ioctl(SIOCGIFMTU) request!";

    ::memset((char *)&buf, 0xAA, sizeof(buf));

    // write() raises SIGPIPE, so ignore it
    auto *old_handler = ::signal(SIGPIPE, SIG_IGN);

    rc = ::write(fd, buf, sizeof(buf));
    errno__ = errno;
    EXPECT_EQ(-1, rc) << "write() must fail";
    EXPECT_EQ(EPIPE, errno__) << "errno should be EPIPE";

    rc = ::write(canfd, buf, sizeof(buf)-1); // invalid size -> fails on mocked can
    errno__ = errno;
    EXPECT_EQ(-1, rc) << "write() must fail";
    EXPECT_EQ(ENOBUFS, errno__) << "errno should be ENOBUFS";

    // read() expect ETIMEDOT errno
    rc = ::read(fd, &buf, sizeof(buf));
    errno__ = errno;
    EXPECT_EQ(-1, rc) << "read() must fail";
    EXPECT_EQ(ENOTCONN, errno__) << "errno should be ENOTCONN";

    // read expect ETIMEDOT errno
    rc = ::read(canfd, &buf, sizeof(buf)-1); // invalid size -> fails on mocked can
    errno__ = errno;
    EXPECT_EQ(-1, rc) << "read() must fail";
    EXPECT_EQ(ENOBUFS, errno__) << "errno should be ENOBUFS";

    // restore previous handler
    ::signal(SIGPIPE, old_handler);

    // test singleton cansim socket.. no other PF_CAN socket can be opened from the process
    for (int i=0; i<10; i++) {
        int f = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        int errno__ = errno;
        EXPECT_EQ(ENODEV, errno__) << "Mocked errno unexpected";
        EXPECT_EQ(-1, f) << "Mocked SocketCAN should fail after 1 opened CAN socket";
    }
    rc = ::close(canfd);
    EXPECT_EQ(0, rc) << "SocketCAN close() failed";

    rc = ::close(fd);
    EXPECT_EQ(0, rc) << "libc close() failed";

    rc = if_nametoindex(NULL);
    errno__ = errno;
    EXPECT_TRUE(rc == -1 && errno__ == EINVAL) << "Invalid string handling";

    // also test logging to file
    ::setenv("CANSIM_LOG", "/tmp/cansim.log", true);
    ::sim_fini();
    ::sim_init();

    rc = if_nametoindex("can1024");
    EXPECT_EQ(rc, 42) << "Expected hadrdoded value for mocket can* interfaces";
    rc = if_nametoindex("vcan1024");
    EXPECT_EQ(rc, 43) << "Expected hadrdoded value for mocket can* interfaces";

    rc = if_nametoindex("lo");
    EXPECT_GE(rc, 1) << "libc if_nametoindex('lo') positive";

    ::unsetenv("CANSIM_LOG");
    ::sim_fini();
    ::sim_init();

    mutex.unlock();
}

TEST_F(SeatCtrlIntegrationTest, ExtraCoverage) {
    mutex.lock();
    std::cout << "[ExtraCoverage] Started ..." << std::endl;

    // enable all posible debugs for code coverage...
    ::setenv("SC_CAN", "vcan123456789abcef", true); // invalid length, max 16
    ::setenv("SC_TIMEOUT", "50", true);
    ::setenv("SC_RPM", "1", true);
    ::setenv("SC_CTL", "1", true);
    ::setenv("SC_STAT", "1", true);
    ::setenv("SC_RAW", "1", true);
    ::setenv("SC_VERBOSE", "1", true);


    ::setenv("SAE_DEBUG", "1", true);
    ::setenv("SAE_VERBOSE", "1", true);
    ::setenv("SAE_POS", "255", true); // start from invalid pos
    ::setenv("SAE_LRN", "0", true); // start from not learned state
    ::setenv("SAE_DELAY", "1", true);

    EXPECT_EQ(0, seatctrl_default_config(&config));
    EXPECT_EQ(0, seatctrl_init_ctx(&ctx, &config));
    EXPECT_EQ(0, seatctrl_open(&ctx)); // on invalid can if

    int target_pos = 14;
    EXPECT_EQ(0, seatctrl_set_position(&ctx, target_pos));
    ::usleep(100 * 1000L); // give ctl time to read next can frame to update motor1_mov_state
    EXPECT_EQ(0, seatctrl_close(&ctx));
    EXPECT_EQ(-EINVAL, seatctrl_close(nullptr));

    mutex.unlock();
}

}  // namespace test
}  // namespace sdv
