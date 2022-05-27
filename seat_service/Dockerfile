# /********************************************************************************
# * Copyright (c) 2022 Contributors to the Eclipse Foundation
# *
# * See the NOTICE file(s) distributed with this work for additional
# * information regarding copyright ownership.
# *
# * This program and the accompanying materials are made available under the
# * terms of the Apache License 2.0 which is available at
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * SPDX-License-Identifier: Apache-2.0
# ********************************************************************************/


FROM --platform=$BUILDPLATFORM alpine:latest as builder

ARG TARGETPLATFORM

WORKDIR /workspace

COPY seat_service/bin_vservice-seat_*_release.* /workspace

# FIXME! Binaries are built from another base image, potentially incompatible with ubuntu:latest image
# RUN tar -xf bin_vservice-seat_x86_64_release.tar.gz && tar -xf bin_vservice-seat_aarch64_release.tar.gz

RUN if [ "$TARGETPLATFORM" = "linux/amd64" ] ; \
    then tar -xf bin_vservice-seat_x86_64_release.tar.gz && cp -rv ./target/x86_64/release/install/ ./bins; \
    else tar -xf bin_vservice-seat_aarch64_release.tar.gz && cp -rv ./target/aarch64/release/install/ ./bins; fi

FROM --platform=$TARGETPLATFORM ubuntu:latest as runtime

LABEL org.opencontainers.image.description "VAL Seat Service container, providing VSC Seat impl and supporting simulated/real can ECU"

## Uncomment for ip/can tools
#RUN apt-get -qqy update && apt-get install -qqy net-tools can-utils iproute2 && apt clean && rm -rf /var/lib/apt/lists/*
# make sure localhost can be resolved!
RUN cat /etc/hosts | grep -q localhost || echo "127.0.0.1 localhost" >> /etc/hosts

COPY --from=builder workspace/bins /app

WORKDIR /app/bin

### "cansim" is special value for using SeatAdjuster CAN simulator (even without vcan support in container)
ENV CAN=cansim

### [vxcan] options
# By default, wait for can-forward.sh to move vxcan1 in container's namespace after container start
#ENV CAN=vxcan1
# If set, val_start.sh shall wait up to specified time (sec) for can interface to appear in the container.
#ENV CAN_WAIT=30

# Seat Service GRPC bind host
ENV SERVICE_HOST=0.0.0.0
# SeatService GRPC bind port
ENV SERVICE_PORT=50051
EXPOSE 50051

# Override this variable to change DataBroker address. Port can be also overridden by DAPR_GRPC_PORT variable.
ENV BROKER_ADDR=0.0.0.0:55555

CMD [ "./val_start.sh" ]
