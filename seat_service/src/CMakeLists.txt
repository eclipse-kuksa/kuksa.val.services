#********************************************************************************
# Copyright (c) 2022 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License 2.0 which is available at
# http://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
#*******************************************************************************/

cmake_minimum_required(VERSION 3.10)

project(SdvVehicleServices C CXX)
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
set (CMAKE_CXX_STANDARD 14)

add_subdirectory(bin/seat_service)
add_subdirectory(lib/seat_adjuster)
add_subdirectory(lib/grpc_services)
add_subdirectory(lib/broker_feeder)

# Examples
add_subdirectory(examples/seat_svc_client)
add_subdirectory(examples/broker_feeder)

#add_subdirectory(lib/can_helpers)
#add_subdirectory(examples/can_send)
#add_subdirectory(examples/can_subscribe)

if (SDV_BUILD_TESTING)
  #add_subdirectory(tests)
endif()
