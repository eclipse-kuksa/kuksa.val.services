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

cmake_minimum_required(VERSION 3.14)

project(SdvSeatAdjusterCAN C CXX)

set (CMAKE_CXX_STANDARD 11)

# Conditional dumps for relative project paths
if (SDV_CMAKE_DEBUG)
  message("--- SeatCtrl PROJECT: ${PROJECT_NAME}")
  message("---   PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
  message("---   CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
  message("---   CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
  message("---   CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")
  message("---   CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}")
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Code coverage
if (SDV_COVERAGE)
  message("--- Using CodeCoverage")
  include (CodeCoverage)
  append_coverage_compiler_flags()
  set(GCOVR_ADDITIONAL_ARGS "-s") # --json coverage.json --json-pretty
else()
  message("--- CodeCoverage disabled! Use 'cmake -DSDV_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug' to enable")
endif()

# System Libraries
include(GNUInstallDirs)

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# stripped down dbc for SECU1_XXX Signals
set(DBC_FILE ${PROJECT_SOURCE_DIR}/seat_ecu.dbc CACHE FILEPATH "DBC file")

set(CANTOOLS_GENERATED_H "${CMAKE_CURRENT_BINARY_DIR}/generated/CAN.h")
set(CANTOOLS_GENERATED_C "${CMAKE_CURRENT_BINARY_DIR}/generated/CAN.c")

# Generated sources
add_custom_target(cangen ALL DEPENDS "${CANTOOLS_GENERATED_C}" "${CANTOOLS_GENERATED_H}")

# generate c stubs from dbc
message("--- Generating can C sources from: ${DBC_FILE}")
add_custom_command(OUTPUT "${CANTOOLS_GENERATED_C}" "${CANTOOLS_GENERATED_H}"
      COMMAND cantools
      ARGS
        generate_c_source
        --no-floating-point-numbers
        --database-name CAN
        -o ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${DBC_FILE}
      DEPENDS "${DBC_FILE}"
)

file(GLOB GEN_SRC ${CMAKE_CURRENT_BINARY_DIR}/generated/*.c ${CMAKE_CURRENT_BINARY_DIR}/generated/*.h )
file(GLOB_RECURSE SOURCES RELATIVE "${CMAKE_CURRENT_BINARY_DIR}/generated" "*.c" "*.h")
message("--- Generated sources: ${GEN_SRC}")

### target: seat_controller_lib
add_library(seat_controller_lib
  "${CANTOOLS_GENERATED_C}"
  seat_controller.cc
)

# fail compilation on any warning
target_compile_options(seat_controller_lib PRIVATE
  -Werror -Wall -Wextra -pedantic
  -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
)
target_include_directories(seat_controller_lib
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
)
set_target_properties(seat_controller_lib PROPERTIES PUBLIC_HEADER "seat_controller.h")

### target: seat_controller
add_executable(seat_controller
  main.cc
)
# fail compilation on any warning
target_compile_options(seat_controller PRIVATE -Werror -Wall -Wextra -pedantic)
target_link_libraries(seat_controller
  seat_controller_lib
  ${CMAKE_THREAD_LIBS_INIT}
)
#install(TARGETS seat_controller DESTINATION bin)

# SocketCAN mock with SeatAdjuster ECU engine simulator
add_subdirectory(tests/cansim)

# Install tools and readmes
install(PROGRAMS
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cansim
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/ecu-reset
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/cangen-SECU1_STAT
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/sim-SECU1_STAT
  DESTINATION bin/tools)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/README.md
  DESTINATION bin/tools)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/cansim/README.md
  DESTINATION bin/tools
  RENAME README-cansim.md)

install(PROGRAMS
  ${CMAKE_CURRENT_SOURCE_DIR}/tools/val_start.sh
  DESTINATION bin)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  DESTINATION bin)


# enable testing
if (SDV_BUILD_TESTING)
  message("--- Tests are enabled!")

  include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
  find_package(GTest)
  include(CTest)
  include(GoogleTest)

  enable_testing()

  add_subdirectory(tests)
else()
  message("--- Tests are disabled! Use cmake -DSDV_BUILD_TESTING=ON to enable")
endif()

