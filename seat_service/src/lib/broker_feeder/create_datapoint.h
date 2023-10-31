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

using sdv::databroker::v1::Datapoint;
using sdv::databroker::v1::Datapoint_Failure_INVALID_VALUE;
using sdv::databroker::v1::Datapoint_Failure_NOT_AVAILABLE;
using sdv::databroker::v1::Int32Array;
using sdv::databroker::v1::Uint32Array;

Datapoint createInvalidValue() {
    Datapoint datapoint;
    datapoint.set_failure_value(Datapoint_Failure_INVALID_VALUE);
    return datapoint;
}

Datapoint createNotAvailableValue() {
    Datapoint datapoint;
    datapoint.set_failure_value(Datapoint_Failure_NOT_AVAILABLE);
    return datapoint;
}

Datapoint createDatapoint(bool value) {
    Datapoint datapoint;
    datapoint.set_bool_value(value);
    return datapoint;
}

Datapoint createDatapoint(int32_t value) {
    Datapoint datapoint;
    datapoint.set_int32_value(value);
    return datapoint;
}

Datapoint createDatapoint(uint32_t value) {
    Datapoint datapoint;
    datapoint.set_uint32_value(value);
    return datapoint;
}

Datapoint createDatapoint(int64_t value) {
    Datapoint datapoint;
    datapoint.set_int64_value(value);
    return datapoint;
}

Datapoint createDatapoint(uint64_t value) {
    Datapoint datapoint;
    datapoint.set_uint64_value(value);
    return datapoint;
}

Datapoint createDatapoint(float value) {
    Datapoint datapoint;
    datapoint.set_float_value(value);
    return datapoint;
}

Datapoint createDatapoint(double value) {
    Datapoint datapoint;
    datapoint.set_double_value(value);
    return datapoint;
}

Datapoint createDatapoint(const std::string& value) {
    Datapoint datapoint;
    datapoint.set_string_value(value);
    return datapoint;
}

Datapoint createDatapoint(std::vector<int32_t> values_array) {
    Datapoint datapoint;
    Int32Array *marray = datapoint.mutable_int32_array();
    auto mvalues = marray->mutable_values();
    for (auto const v: values_array) {
        mvalues->Add(v);
    }
    return datapoint;
}

Datapoint createDatapoint(std::vector<uint32_t> values_array) {
    Datapoint datapoint;
    Uint32Array *marray = datapoint.mutable_uint32_array();
    auto mvalues = marray->mutable_values();
    for (auto const v: values_array) {
        mvalues->Add(v);
    }
    return datapoint;
}

}  // namespace broker_feeder
}  // namespace sdv
