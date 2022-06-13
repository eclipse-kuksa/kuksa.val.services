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
cmake_minimum_required(VERSION 3.5.1)

# Project
project(SdvHAL C CXX)
# use conan as dependency management
include(${CMAKE_BINARY_DIR}/conan_paths.cmake)

option(SDV_BUILD_TESTING "Build with Tests" ON)

message("--- CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}")

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x600)
endif()

# enable testing
if (SDV_BUILD_TESTING)
  message(STATUS "Build tests ON")
  enable_testing()
  include(CTest)
  find_package(GTest)
else()
  message(STATUS "Build tests OFF")
endif()

add_subdirectory(src)

