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

add_executable(seat_svc_client
  "seat_svc_client.cc"
)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)

target_include_directories(seat_svc_client
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/../../lib/grpc_services/seats_grpc_service/" # generated *.pb.h files
)

target_link_libraries(seat_svc_client
  comfort_seats_grpc_service
)

install(
  TARGETS seat_svc_client
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
