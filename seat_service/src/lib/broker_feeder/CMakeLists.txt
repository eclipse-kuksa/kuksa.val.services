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

# Data Broker Proto file
set(PROTO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../vehicle_data_broker/proto/)

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

add_library(data_broker_feeder
  STATIC
    data_broker_feeder.cc
)

target_link_libraries(data_broker_feeder
  PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
)

target_include_directories(data_broker_feeder
  PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# protobuf
protobuf_generate(
  TARGET
    data_broker_feeder
  LANGUAGE
    cpp
  PROTOS
    ${PROTO_SOURCE_DIR}/sdv/databroker/v1/broker.proto
    ${PROTO_SOURCE_DIR}/sdv/databroker/v1/collector.proto
    ${PROTO_SOURCE_DIR}/sdv/databroker/v1/types.proto
  IMPORT_DIRS
    ${PROTO_SOURCE_DIR}
)

# GRPC services
protobuf_generate(
  TARGET
    data_broker_feeder
  LANGUAGE
    grpc
  GENERATE_EXTENSIONS
    .grpc.pb.h
    .grpc.pb.cc
  PROTOS
    ${PROTO_SOURCE_DIR}/sdv/databroker/v1/collector.proto
    ${PROTO_SOURCE_DIR}/sdv/databroker/v1/broker.proto
  PLUGIN
    "protoc-gen-grpc=${GRPC_CPP_PLUGIN_PROGRAM_ENV}"
  IMPORT_DIRS
    ${PROTO_SOURCE_DIR}
)

target_link_libraries(data_broker_feeder
  PUBLIC
    seat_adjuster
)
