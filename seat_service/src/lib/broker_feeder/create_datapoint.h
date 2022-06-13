/********************************************************************************
* Copyright (c) 2022 Contributors to the Eclipse Foundation
*
* See the NOTICE file(s) distributed with this work for additional
* information regarding copyright ownership.
*
* This program and the accompanying materials are made available under the
* terms of the Apache License 2.0 which is available at
* http://www.apache.org/licenses/LICENSE-2.0
*
* SPDX-License-Identifier: Apache-2.0
********************************************************************************/
/**
 * @file      create_datapoint.h
 * @brief     File contains helper functions to create a Datapoint structure
 *
 */
#pragma once

#include "sdv/databroker/v1/types.pb.h"

namespace sdv {
namespace broker_feeder {

sdv::databroker::v1::Datapoint createInvalidValue() {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_failure_value(sdv::databroker::v1::Datapoint_Failure_INVALID_VALUE);
    return datapoint;
}

sdv::databroker::v1::Datapoint createNotAvailableValue() {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_failure_value(sdv::databroker::v1::Datapoint_Failure_NOT_AVAILABLE);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(bool value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_bool_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(int32_t value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_int32_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(uint32_t value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_uint32_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(int64_t value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_int64_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(uint64_t value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_uint64_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(float value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_float_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(double value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_double_value(value);
    return datapoint;
}

sdv::databroker::v1::Datapoint createDatapoint(const std::string& value) {
    sdv::databroker::v1::Datapoint datapoint;
    datapoint.set_string_value(value);
    return datapoint;
}

}  // namespace broker_feeder
}  // namespace sdv
