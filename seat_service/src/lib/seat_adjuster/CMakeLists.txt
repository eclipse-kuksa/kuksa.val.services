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

# seat service
add_library(seat_adjuster
  "seat_adjuster.cc"
)

target_link_libraries(seat_adjuster
  seat_controller_lib
)

target_include_directories(seat_adjuster
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_subdirectory(seat_controller)
