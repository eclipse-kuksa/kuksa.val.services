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

project(SwdVCANSim C)

# option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# Conditional dumps for relative project paths
if (SDV_CMAKE_DEBUG)
  message("---- CANSIM PROJECT: ${PROJECT_NAME}")
  message("---- CANSIM CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
  message("----  CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
  message("----  PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
endif()

# cansim has to be a shared library for LD_PRELOAD
add_library(cansim SHARED
  "cansim_lib.c"
  "seatadjuster_engine.c"
)

target_include_directories(cansim
  PUBLIC
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# fail compilation on any warning
target_compile_options(cansim PRIVATE -Wall -Wextra -pedantic) # -Werror

#SET(GCC_SANITIZE_FLAGS "-Wpedantic -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_SANITIZE_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${GCC_SANITIZE_FLAGS}")

set_property(TARGET cansim PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(cansim
  pthread
  dl
)
# install in the same library as cansim script
install(TARGETS cansim DESTINATION bin/tools)

add_executable(cansim_test
  "main.c"
)

# fail compilation on any warning
target_compile_options(cansim_test PRIVATE -Werror -Wall -Wextra -pedantic)

target_include_directories(cansim_test
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(cansim_test
  cansim
)
