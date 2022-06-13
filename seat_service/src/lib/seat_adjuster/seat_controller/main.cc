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
 * @file      main.cc
 * @brief     File contains standalone test for seat_controller
 */

#include <stdio.h>
#include <stdlib.h>

#include "seat_controller.h"

seatctrl_config_t config = {
    "vcan0",
    false,
    true,
    true,
    true,
    DEFAULT_OPERATION_TIMEOUT,
    DEFAULT_RPM
};

/*
seatctrl_config_t config = {
    .can_device = "vcan0",
    .debug_raw = false,
    .debug_ctl = true,
    .debug_stats = true,
    .debug_verbose = true,
    .command_timeout = DEFAULT_OPERATION_TIMEOUT,
    .motor_rpm = DEFAULT_RPM
};
*/
seatctrl_context_t ctx;

/**
 * @brief
 *
 * @param position
 */
void pos_cb(SeatCtrlEvent event, int value, void* ctx) {
    if (event == SeatCtrlEvent::Motor1Pos) {
        printf("****** motor1 pos changed: %3d%%, ctx:%p\n", value, ctx);
    } else
    if (event == SeatCtrlEvent::CanError) {
        printf("****** Can error: %d, ctx:%p\n", value, ctx);
    }

}


/**
 * @brief main function
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char **argv) {
    int pos;
    int rc;

#if 0
    return seatctrl_test(argc, argv);
#endif

    printf("\n=== Seat Adjuster controller ===\n");

    // init default config from env variables
    printf("\n=== seatctrl_default_config ===\n");
    rc = seatctrl_default_config(&config);
    if (rc != 0) printf("rc: %d\n", rc);
    // override config in code if needed, e.g. with with argv[1] if present
    if (argc > 1) {
        char* arg=argv[1];
        if (arg) {
            config.can_device = arg;
        }
    }

    printf("\n=== seatctrl_init_ctx [%s] ===\n", config.can_device);
    rc = seatctrl_open(&ctx);
    if (rc != 0) printf("rc: %d\n", rc);
    rc = seatctrl_init_ctx(&ctx, &config);
    if (rc != 0) printf("rc: %d\n", rc);
    rc = seatctrl_open(&ctx);
    if (rc != 0) printf("rc: %d\n", rc);
    usleep(1 * 1000000L);

    rc = seatctrl_set_event_callback(&ctx, pos_cb, NULL);
    if (rc != 0) printf("rc: %d\n", rc);
    // verify incoming data
    pos = seatctrl_get_position(&ctx);
    printf("\n=== seatctrl_get_position() -> %d ===\n\n", pos);
    usleep(1 * 1000000L);

    // initial pos, 2nd pos override-able by envvars
    int pos_1 = 42;
    int pos_2 = 100;
    if (getenv("SC_POS1")) pos_1 = atoi("SC_POS1");
    if (getenv("SC_POS2")) pos_2 = atoi("SC_POS2");

    printf("\n=== seatctrl_set_position(%d) ===\n\n", pos_1);
    rc = seatctrl_set_position(&ctx, pos_1);
    if (rc != 0) printf("rc: %d\n", rc);
    usleep(2 * 1000000L);

    // set to same position
    printf("\n=== seatctrl_set_position(%d) [again] ===\n\n", pos_1);
    rc = seatctrl_set_position(&ctx, 42);
    if (rc != 0) printf("rc: %d\n", rc);
    usleep(10 * 1000000L);

    // set to new position
    printf("\n\n=== seatctrl_set_position(%d) ===\n\n", pos_2);
    rc = seatctrl_set_position(&ctx, pos_2);
    if (rc != 0) printf("rc: %d\n", rc);

    printf("\n=== wait: 60s ===\n\n");
    usleep(60 * 1000000L);

    pos = seatctrl_get_position(&ctx);
    printf("\n=== seatctrl_get_position() -> %d ===\n", pos);

    printf("\n=== seatctrl_close() ===\n");
    rc = seatctrl_close(&ctx);

    return 0;
}
