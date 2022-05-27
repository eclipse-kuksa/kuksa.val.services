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

# Conditional dumps for relative project paths
if (SDV_CMAKE_DEBUG)
  message("---- SeatCtrl TEST PROJECT: ${PROJECT_NAME}")
  message("----   PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
  message("----   CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")
  message("----   CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
  message("----   CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}")
  message("----   CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}")
endif()

### target: TestSeatCtrlApi
add_executable(testrunner_seatctrl
  mock/mock_unix_socket.cc
  test_seatctrl_api.cc
)
target_include_directories(testrunner_seatctrl
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/mock
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../generated
)
# fail compilation on any warning
target_compile_options(testrunner_seatctrl PRIVATE
  -Werror -Wall -Wextra -pedantic
)

target_link_libraries(testrunner_seatctrl
  PRIVATE
    seat_controller_lib
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    pthread
)
gtest_add_tests(TARGET testrunner_seatctrl)

#################################
### target: integration tests ###
#################################

add_executable(integration_test
  integration_seatctrl.cc
)
target_include_directories(integration_test
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${PROJECT_SOURCE_DIR}/generated
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cansim
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mock
)
# fail compilation on any warning
target_compile_options(integration_test PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(integration_test
  PRIVATE
    seat_controller_lib
    cansim  # WARNING: linking to this lib overrides socket(), read(), write() system calls for the binary!
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    pthread
)

gtest_add_tests(TARGET integration_test)

if (SDV_COVERAGE)

  #set(CODE_COVERAGE_VERBOSE "ON")

  # helper target for cleaning gcov artifacts
  add_custom_target(coverage_clean
    # COMMAND ${CMAKE_MAKE_PROGRAM} clean
    COMMAND find . -type f -name *.gcda -delete -print
    COMMAND find . -type f -name *.gcno -delete -print
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  SET(COVERAGE_EXCLUDE
    "${PROJECT_SOURCE_DIR}/main.cc"
    "${PROJECT_SOURCE_DIR}/tests/cansim/main.c"
    "${PROJECT_SOURCE_DIR}/tests/mock/*"
    "${PROJECT_SOURCE_DIR}/tests/integration_*"
    "${PROJECT_SOURCE_DIR}/tests/test_*"
    "${CMAKE_BINARY_DIR}/*"
  )

  SET(COVERAGE_DEPS
    "testrunner_seatctrl"
    "integration_test"
  )

  setup_target_for_coverage_gcovr_html(
    NAME report_codecov_vservice-seat-ctrl_html   # New target name
    EXECUTABLE ctest -j ${PROCESSOR_COUNT}        # Executable in PROJECT_BINARY_DIR
    BASE_DIRECTORY ".."                           # Base directory for report
    DEPENDENCIES ${COVERAGE_DEPS}                 # Dependencies to build first
    EXCLUDE ${COVERAGE_EXCLUDE}
  )
  setup_target_for_coverage_lcov(
    NAME report_codecov_vservice-seat-ctrl_lcov   # New target name
    EXECUTABLE ctest -j ${PROCESSOR_COUNT}        # Executable in PROJECT_BINARY_DIR
    BASE_DIRECTORY ".."                           # Base directory for report
    DEPENDENCIES ${COVERAGE_DEPS}                 # Dependencies to build first
    EXCLUDE ${COVERAGE_EXCLUDE}
    LCOV_ARGS "--no-external"                     # exclude /usr/include
  )
  setup_target_for_coverage_gcovr_xml(
    NAME report_codecov_vservice-seat-ctrl    # New target name
    EXECUTABLE ctest -j ${PROCESSOR_COUNT}    # Executable in PROJECT_BINARY_DIR # -T memcheck
    BASE_DIRECTORY ".."                       # Base directory for report
    DEPENDENCIES ${COVERAGE_DEPS}             # Dependencies to build first
    EXCLUDE ${COVERAGE_EXCLUDE}
  )
endif()
