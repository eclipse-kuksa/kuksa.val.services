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

add_executable(seat_service
  "seat_data_feeder.cc"
  "main.cc"
)

target_link_libraries(seat_service
  comfort_seats_grpc_service
  data_broker_feeder
)

install(
  TARGETS seat_service
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
