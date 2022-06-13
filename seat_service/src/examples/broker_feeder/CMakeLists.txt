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

add_executable(broker_feeder
  "broker_feeder.cc"
)

find_package(gRPC REQUIRED)
find_package(Protobuf REQUIRED)

target_link_libraries(broker_feeder
  data_broker_feeder
)

install(
  TARGETS broker_feeder
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
