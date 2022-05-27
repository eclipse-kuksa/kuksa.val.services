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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cansim_lib.h"

#include <string.h>
#include <linux/can.h>
#include <net/if.h>
#include <sys/ioctl.h>

// private function declarations (not in header)
int sae_estimate_move_time(int rpm);
int sae_pos_increment(sae_context_t *ctx);

#define POS_SHIFT 10 // 2^10 = 1024
#define POS_MASK  ((1 << POS_SHIFT)-1) // fractional point mask

static inline int sae_pos_raw(int percent) {
    return percent << POS_SHIFT;
}

static inline int sae_pos_percent(int raw) {
    return raw >> POS_SHIFT;
}

int main(int argc, char* argv[])
{
    // if (argc < 2) {
    //     fprintf(stderr, "Usage %s cmd arg0 arg1 ...\n", argv[0]);
    //     exit(1);
    // }

    //_initialize();
    setenv("SAE_DELAY", "500", true);
    setenv("SAE_ALL", "1", true);
    setenv("SAE_POS", "2", true);

    sim_context_t* ctx = sim_context();
    (void)ctx;
    (void)argc;
    (void)argv;

    // fprintf(stderr, "Executing `");
    // for (int i = 1; i < argc; i++) {
    //     if (i < argc - 1) {
    //         fprintf(stderr, "%s ", argv[i]);
    //     } else {
    //         fprintf(stderr, "%s", argv[i]);
    //     }
    // }
    // fprintf(stderr, "` with SocketCAN simulation ...\n");

    ctx->mock_active = true;

    if_nametoindex("can0");
    if_nametoindex("vcan0");

    _sae_verbose = true;
    _sae_debug = true;


    {
        struct sockaddr_can addr;
	    struct ifreq ifr;
        int rc;

/*
        uint8_t can_buffer_rx[16] = { // 712#46.44.01.00.00.00.00.0
            0x12, 0x07, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x46, 0x44, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
        };
*/
        uint8_t can_buffer_rx[16];
        const uint8_t can_buffer_tx[16] = { // 705#02.1E.00.00.00.00.00.00
            0x05, 0x07, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x96, 0x1e, 0x50, 0x1e, 0x50, 0x00, 0x00, 0x00
        };
        struct can_frame frame;

        int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	    if (s < 0) {
		    perror("socket: ");
            return s;
        }

        strncpy(ifr.ifr_name, "CANSIM", IFNAMSIZ-1); // max 16!
        rc = ioctl(s, SIOCGIFINDEX, &ifr);
        if (rc == -1) {
            perror("ioctl(SIOCGIFINDEX): ");
            ifr.ifr_ifindex = -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind: ");
        }

        for (int r=0; r<255; r+=10) {
            ctx->sae._sim_motor1_rpm = r;
            int inc = sae_pos_increment(&ctx->sae);
            printf("  rpm(%3d): total: %dms, inc:0x%0x (%f)\n", r, sae_estimate_move_time(r), inc, inc / (double)(1<<POS_SHIFT));
        }

        memset(&frame, 0, sizeof(frame));
        frame.can_dlc = 8;
        frame.can_id = 0x705;
        rc = write(s, &can_buffer_tx, sizeof(can_buffer_tx));
        if (rc == -1) {
            perror("write:");
        }

        for (int i=0; i<20; i++) {
            printf("can_read #%2d\n", i);
            rc = read(s, &can_buffer_rx, sizeof(can_buffer_rx));
            if (rc == -1) {
                perror("read:");
            }
        }
        rc = close(s);
        if (!rc) perror("close: ");
    }

    return 0;
    //return execvp(argv[1], argv+1);
}