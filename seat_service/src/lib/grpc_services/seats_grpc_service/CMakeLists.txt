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

# Proto file
set(PROTO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../proto/)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Needed for cross build, env vars set via conan env_generator
find_program(PROTOC_PROGRAM_TEMP
  NAMES protoc 
  PATHS ENV PATH 
  NO_DEFAULT_PATH
)
get_filename_component(PROTOC_PROGRAM_TEMP "${PROTOC_PROGRAM_TEMP}" ABSOLUTE)
set_property(TARGET protobuf::protoc PROPERTY IMPORTED_LOCATION ${PROTOC_PROGRAM_TEMP})

# Needed for cross build, env vars set via conan env_generator
#   Do not use previously found program (GRPC_CPP_PLUGIN_PROGRAM) but new varaible: GRPC_CPP_PLUGIN_PROGRAM_ENV
find_program(GRPC_CPP_PLUGIN_PROGRAM_ENV
  NAMES grpc_cpp_plugin 
  PATHS ENV PATH 
  NO_DEFAULT_PATH
)
get_filename_component(GRPC_CPP_PLUGIN_PROGRAM_ENV "${GRPC_CPP_PLUGIN_PROGRAM_ENV}" ABSOLUTE)

add_library(comfort_seats_grpc_service 
    STATIC
        seats_grpc_service.cc 
)
target_link_libraries(comfort_seats_grpc_service 
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
        gRPC::grpc++_reflection
)

target_include_directories(comfort_seats_grpc_service
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)


# protobuf
protobuf_generate(
    TARGET 
        comfort_seats_grpc_service 
    LANGUAGE 
        cpp
    PROTOS
        ${PROTO_SOURCE_DIR}/sdv/edge/comfort/seats/v1/seats.proto
    IMPORT_DIRS
        ${PROTO_SOURCE_DIR}
)

# "sdv/" ABSOLUTE)


# GRPC services
protobuf_generate(
    TARGET
        comfort_seats_grpc_service
    LANGUAGE
        grpc
    GENERATE_EXTENSIONS
        .grpc.pb.h
        .grpc.pb.cc
    PROTOS
        ${PROTO_SOURCE_DIR}/sdv/edge/comfort/seats/v1/seats.proto
    IMPORT_DIRS
        ${PROTO_SOURCE_DIR}
    PLUGIN
        "protoc-gen-grpc=${GRPC_CPP_PLUGIN_PROGRAM_ENV}"
)

target_link_libraries(comfort_seats_grpc_service
    PUBLIC
        seat_adjuster
)
