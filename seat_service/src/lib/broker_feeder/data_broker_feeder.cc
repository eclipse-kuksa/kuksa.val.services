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
 * @file      data_broker_feeder.cc
 * @brief     File contains implementation of the generic class DataBrokerFeeder.
 *
 */
#include "data_broker_feeder.h"

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>

#include "kuksa_client.h"
#include "sdv/databroker/v1/broker.grpc.pb.h"
#include "sdv/databroker/v1/collector.grpc.pb.h"

namespace sdv {
namespace broker_feeder {

// allow suppressing multi line dumps from DataBrokerFeederImpl
static int dbf_debug = std::stoi(sdv::utils::getEnvVar("DBF_DEBUG", "1"));

using DatapointId = google::protobuf::int32;

class DataBrokerFeederImpl final:
    public DataBrokerFeeder
{
private:
    const GrpcMetadata grpc_metadata_;
    const DatapointConfiguration dp_config_;
    DatapointValues stored_values_;
    google::protobuf::Map<std::string, DatapointId> id_map_;
    DatabrokerMetadata dp_meta_;

    std::atomic<bool> feeder_active_;
    std::atomic<bool> feeder_ready_;
    std::mutex stored_values_mutex_;
    std::condition_variable feeder_thread_sync_;

    std::shared_ptr<KuksaClient> client_;
    std::unique_ptr<grpc::ClientContext> subscriber_context_;

   public:
    DataBrokerFeederImpl(std::shared_ptr<KuksaClient> client, DatapointConfiguration&& dp_config)
        : client_(client)
        , dp_config_(std::move(dp_config))
        , dp_meta_()
        , feeder_active_(true)
        , feeder_ready_(false) {}

    ~DataBrokerFeederImpl() { Shutdown(); }

    void Run() override {
        /* This thread is responsible for establishing a connection to the data broker.
         * Once connection is present, it starts registering the data points (metadata)
         * with the broker and feeds the initial values (plus possible already stored values
         * to the broker.
         * Afterwards it is forwarding values stored by the feeding methods and tries
         * re-establishing a lost connection to the broker.
         */
        while (feeder_active_) {
            if (dbf_debug > 0) {
                std::cout << "DataBrokerFeeder: Connecting to data broker ..." << std::endl;
            }
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
            client_->WaitForConnected(deadline);
            if (client_->Connected()) {
                std::cout << "DataBrokerFeeder: Connected to databroker." << std::endl;
            }
            if (feeder_active_ && client_->Connected()) {
                if (!registerDatapoints()) {
                    // don't attempt to feed values (too often) if registration status was an error
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }
            }
            feeder_ready_ = true;
            bool also_feed_initial_values = true;
            while (feeder_active_ && client_->Connected()) {
                feedStoredValues(also_feed_initial_values);
                also_feed_initial_values = false;

                if (dbf_debug > 6) {
                    std::ostringstream os;
                    os << "DataBrokerFeeder: Run() ["
                        << "active:" << std::boolalpha << feeder_active_
                        << ", connected:" << std::boolalpha << client_->Connected()
                        << ", state:" << " " << sdv::utils::toString(client_->GetState())
                        << "]";
                    std::cout << os.str() << std::endl;
                }

                if (feeder_active_ && client_->Connected()) {
                    std::unique_lock<std::mutex> lock(stored_values_mutex_);
                    if (stored_values_.empty()) {
                        if (dbf_debug > 2) {
                            std::cout << "DataBrokerFeeder: Run() waiting for values..." << std::endl;
                        }
#if 1
                        // replacement for feeder_thread_sync_.wait(lock); block for smaller periods and abort
                        while (feeder_active_) {
                            auto wait_status = feeder_thread_sync_.wait_for(lock, std::chrono::seconds(5));
                            if (!feeder_active_ || !client_->Connected()) {
                                break;
                            }
                            if (wait_status == std::cv_status::no_timeout) {
                                if (dbf_debug > 9) {
                                    std::cout << "DataBrokerFeeder: Run() notified" << std::endl;
                                }
                                break;
                            } else {
                                if (dbf_debug > 9) {
                                    std::cout << "DataBrokerFeeder: timedout. waiting..." << std::endl;
                                }
                                continue;
                            }
                        }
#else
                        feeder_thread_sync_.wait(lock);
                        if (dbf_debug > 3) {
                            std::cout << "DataBrokerFeeder: Run() notified" << std::endl;
                        }
#endif
                    }
                }
                if (!client_->Connected()) {
                    if (dbf_debug > 0) {
                        std::cout << "DataBrokerFeeder: Disconnected!" << std::endl;
                    }
                    break;
                }
            }
            cleanup();
        }
    }

    void cleanup() {
        // reset metadata / id mapping on disconnect!
        if (dbf_debug > 1) {
            std::cout << "DataBrokerFeeder: cleanup cached entries..." << std::endl;
        }
        id_map_.clear();
        dp_meta_.clear();
        feeder_ready_ = false;
    }

    void Shutdown() override {
        if (feeder_active_) {
            std::cout << "DataBrokerFeeder::Shutdown: Waiting for feeder to stop ..." << std::endl;
            {
                std::unique_lock<std::mutex> lock(stored_values_mutex_);
                stored_values_.clear();
                feeder_active_ = false;
            }
            feeder_thread_sync_.notify_all();
            std::cout << "DataBrokerFeeder::Shutdown: Feeder stopped." << std::endl;
        }

        if (subscriber_context_) {
            subscriber_context_->TryCancel();
        }
    }

    bool Ready() const override {
        return feeder_active_ && client_->Connected() && feeder_ready_;
    }

    /** Feed a set ("batch") of datapoint values to the data broker.
     *  If the data broker is currently not connected or another "recoverable"error occurs, the passed
     *  values are stored by the feeder and tried being send, when the connection to the broker could be
     *  established (again).
     */
    void FeedValues(const DatapointValues& values) override
    {
        if (feeder_active_) {
            if (dbf_debug > 1) {
                std::cout << "DataBrokerFeeder::FeedValues: Enqueue " << values.size() << " values" << std::endl;
            }
            std::unique_lock<std::mutex> lock(stored_values_mutex_);
            storeValues(values);
            feeder_thread_sync_.notify_all();
            std::this_thread::yield();
        }
    }

    /** Feed a single datapoint value to the data broker.
     *  (@see FeedValues)
     */
    void FeedValue(const std::string& name, const sdv::databroker::v1::Datapoint& value) override
    {
        if (feeder_active_) {
            if (dbf_debug > 1) {
                std::cout << "DataBrokerFeeder::FeedValue: Enqueue value: { "
                    << value.ShortDebugString()
                    << " } " << std::endl;
            }
            std::unique_lock<std::mutex> lock(stored_values_mutex_);
            storeValue(name, value);
            feeder_thread_sync_.notify_all();
            std::this_thread::yield();
        }
    }

private:
    /** Add the passed values to the stored values (possibly overwriting already stored values) */
    void storeValues(const DatapointValues& values) {
        for (const auto& value : values) {
            storeValue(value.first, value.second);
        }
    }

    /** Add the passed value to the stored values (possibly overwriting an already stored value) */
    void storeValue(const std::string& name, const sdv::databroker::v1::Datapoint& value) {
        stored_values_[name] = value;
    }

    /** Register the data points (metadata) passed to the c-tor with the data broker.
     */
    bool registerDatapoints() {
        if (dbf_debug > 0) {
            std::cout << "DataBrokerFeeder::registerDatapoints()" << std::endl;
        }
        if (checkDatapoints()) {
            std::cout << "DataBrokerFeeder::registerDatapoints() datapoints already registered." << std::endl;
            std::ostringstream os;
            for (const auto& m : dp_meta_) {
                if (dbf_debug > 1) {
                    os << "  [registerDatapoints]  '" << m.first << "' -> id:" << m.second.id() << "\n";
                }
                id_map_[m.first] = m.second.id();
            }
            if (dbf_debug > 1) {
                std::cout << os.str() << std::endl;
            }
            return true;
        }

        sdv::databroker::v1::RegisterDatapointsRequest request;
        for (const auto& metadata : dp_config_) {
            ::sdv::databroker::v1::RegistrationMetadata reg_data;
            reg_data.set_name(metadata.name);
            // reg_data.set_entry_type(metadata.entry_type); // ignored, current proto does not support setting EnrtyType, just getting it
            reg_data.set_data_type(metadata.data_type);
            reg_data.set_change_type(metadata.change_type);
            reg_data.set_description(metadata.description);
            request.mutable_list()->Add(std::move(reg_data));
        }

        auto context = client_->createClientContext();
        sdv::databroker::v1::RegisterDatapointsReply reply;
        grpc::Status status = client_->RegisterDatapoints(context.get(), request, &reply);
        if (dbf_debug > 4) {
            std::ostringstream os;
            os << "[GRPC]  Collector.RegisterDatapoints(" << request.ShortDebugString() << ") -> "
               << sdv::utils::toString(status);
            if (!reply.DebugString().empty()) {
               os << ", reply:\n" << reply.DebugString();
            }
            std::cout << os.str() << std::endl;
        }
        if (status.ok()) {
            std::cout << "DataBrokerFeeder::registerDatapoints: Datapoints registered." << std::endl;
            id_map_ = std::move(*reply.mutable_results());
            std::ostringstream os;
            for (const auto& name_to_id : id_map_) {
                os << "  [registerDatapoints]  '" << name_to_id.first
                   << "' -> id:" << name_to_id.second << "\n";
            }
            std::cout << os.str() << std::endl;
            return true;
        } else {
            std::cerr << "DataBrokerFeeder::registerDatapoints() failed!" << std::endl;
            handleError(status, "DataBrokerFeeder::registerDatapoints");
            return false;
        }
    }

    /**
     * @brief Gets configured (dp_config_) Datapoints Metadata from databroker, results in dp_meta_ map.
     *
     * @return true if metadata was was updated successfully
     */
    bool getMetadata() {
        if (dbf_debug > 0) {
            std::cout << "DataBrokerFeeder::getMetadata(" << dp_config_.size() << ")" << std::endl;
        }
        // Do not get all metadata if nothing is configured!
        if (dp_config_.size() == 0) {
            return false; // we want to update id
        }
        sdv::databroker::v1::GetMetadataRequest request;
        for (const auto& metadata : dp_config_) {
            request.add_names(metadata.name);
        }

        auto context = client_->createClientContext();
        sdv::databroker::v1::GetMetadataReply reply;
        grpc::Status status = client_->GetMetadata(context.get(), request, &reply);
        if (dbf_debug > 4) {
            std::ostringstream os;
            os << "[GRPC]  Broker.GetMetadata(" << request.ShortDebugString() << ") -> "
                << sdv::utils::toString(status);
            if (!reply.DebugString().empty()) {
                os << ", reply:\n" << reply.DebugString();
            }
            std::cout << os.str() << std::endl;
        }
        if (status.ok()) {
            auto metadata = reply.list();
            std::ostringstream os;
            os << "DataBrokerFeeder::getMetadata: Got " << metadata.size() << " entries:\n";
            for (const auto& m : metadata) {
                dp_meta_[m.name()] = m;
                if (dbf_debug > 0) {
                    os << "  [getMetadata]  {"
                    << "name:'" << m.name() << "'"
                    << ", id:" << m.id()
                    << ", type:" << DataType_Name(m.data_type())
                    << ", entry:" << EntryType_Name(m.entry_type())
                    // << ", change:" << m.change_type() << " " << ChangeType_Name(m.change_type()) // NOTE: change_type is always CONTINUOUS at the moment...
                    << ", desc:'" << m.description() << "'"
                    << "}\n";
                }
            }
            if (dbf_debug > 0) {
                std::cout << os.str() << std::endl;
            }
        } else {
            std::cerr << "DataBrokerFeeder::getMetadata() failed!" << std::endl;
            handleError(status, "DataBrokerFeeder::getMetadata");
            return false;
        }
        return true;
    }

    bool checkDatapoints() {
        if (dbf_debug > 1) {
            std::cout << "DataBrokerFeeder::checkDatapoints()" << std::endl;
        }

        getMetadata();

        // check if each DP from dp_config_ is available (warnings if getMeta failed)
        bool result = true;
        for (const auto& dp : dp_config_) {
            auto iter = dp_meta_.find(dp.name);
            if (iter == dp_meta_.end()) {
                std::cerr << "DataBrokerFeeder::checkDatapoints() " << dp.name
                        << " not registered!" << std::endl;
                result = false;
                continue;
            }
            sdv::databroker::v1::Metadata md = iter->second;
            // TODO: sanity check if data is as expected.
            if (dp.data_type != md.data_type()) {
                std::cerr << "DataBrokerFeeder::checkDatapoints() " << dp.name
                        << " has different type:" << DataType_Name(md.data_type()) << std::endl;
                result = false;
            }
        }
        if (dbf_debug > 0) {
            std::cout << "DataBrokerFeeder::checkDatapoints() -> " <<  std::boolalpha << result << std::endl;
        }
        return result;
    }

    /** Feed stored and - on demand - initial values to the data broker.
     *  If for a datapoint an initial as well as a stored value is present, the stored on gets precedence.
     */
    void feedStoredValues(bool feed_initial_values = false) {
        DatapointValues values_to_feed;
        {
            std::unique_lock<std::mutex> lock(stored_values_mutex_);
            values_to_feed.swap(stored_values_);
        }
        if (feed_initial_values) {
            for (const auto& metadata : dp_config_) {
                values_to_feed.insert(std::make_pair(metadata.name, metadata.initial_value));
            }
        }
        bool successfully_sent = feedToBroker(values_to_feed);
        if (!successfully_sent) {
            restoreValues(std::move(values_to_feed));
            // warning: creates busy loop on permanent errrors
            if (feeder_active_ && client_->Connected()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }

    /** Feed the passed values to the data broker. */
    bool feedToBroker(const DatapointValues& values_to_feed) {
        if (dbf_debug > 0) {
            std::cout << "DataBrokerFeeder::feedToBroker: " << values_to_feed.size() << " datapoints" << std::endl;
        }
        sdv::databroker::v1::UpdateDatapointsRequest request;
        std::ostringstream os;
        for (const auto& value : values_to_feed) {
            auto iter = id_map_.find(value.first);
            if (iter != id_map_.end()) {
                auto id = iter->second;
                (*request.mutable_datapoints())[id] = value.second;
                if (dbf_debug > 0) {
                    os << "  [feedToBroker]  '" << value.first << "' id:" << id
                       << ", type:" << value.second.value_case()
                       << ", value: { " << value.second.ShortDebugString() << " }\n";
                }
            } else {
                std::cerr << "  [feedToBroker]  Unknown name '" << value.first << "'!" << std::endl;
            }
        }
        if (dbf_debug > 0) {
            std::cout << os.str() << std::endl;
        }

        auto context = client_->createClientContext();
        sdv::databroker::v1::UpdateDatapointsReply reply;
        grpc::Status status = client_->UpdateDatapoints(context.get(), request, &reply);
        if (dbf_debug > 4) {
            std::ostringstream os;
            os << "[GRPC]  Collector.UpdateDatapoints(" << request.ShortDebugString() << ") -> "
               << sdv::utils::toString(status);
            if (!reply.DebugString().empty()) {
                os << ", reply:\n" << reply.DebugString();
            }
            std::cout << os.str() << std::endl;
        }
        if (status.ok()) {
            // status.ok, but there could be update errors in reply
            bool result = true;
            std::ostringstream os;
            for (const auto& it: reply.errors()) {
                int32_t id = it.first;
                sdv::databroker::v1::DatapointError de = it.second;
                std::string dpName = "Unknown";
                for (const auto& m: id_map_) {
                    if (m.second == id) {
                        dpName = m.first;
                        break;
                    }
                }
                os << "  [feedToBroker]  id:" << id
                   << ", '" << dpName << "', Error: "
                   << DatapointError_Name(de) << "\n";
                result = false;
            }
            if (!result) {
                std::cerr << "DataBrokerFeeder::feedToBroker WARNING: UpdateDatapoints() errors:\n"
                          << os.str() << std::endl;
            }
            // It's more important to show warning to user,
            // if we return false the same invalid datapoints will be sent in a busy loop
            return true; // result;
        }
        handleError(status, "DataBrokerFeeder::feedToBroker");
        return false;
    }

    /** Re-store values on a feeding error; already contained values are rated newer and are not overwritten */
    void restoreValues(DatapointValues&& values) {
        std::unique_lock<std::mutex> lock(stored_values_mutex_);
        stored_values_.insert(values.begin(), values.end());
    }

    /** Log the gRPC error information and
     *   - either trigger re-connection and "recoverable" errors
     *   - or deactivate the feeder.
     */
    void handleError(const grpc::Status& status, const std::string& caller) {
        std::ostringstream os;
        os << caller << " failed:" << std::endl
           << "    ErrorCode: " << status.error_code()
                << " " << sdv::utils::toString(status.error_code()) << "\n"
           << "    ErrorMsg: '" << status.error_message() << "'\n"
           << ( status.error_details().empty() ? "" : "    Details: '" + status.error_details() + "'\n" )
           << "    grpcChannelState: " << client_->GetState() << "\n";
        std::cerr << os.str() << std::endl;

        switch (status.error_code()) {
          case GRPC_STATUS_INTERNAL:
          case GRPC_STATUS_UNAUTHENTICATED:
          case GRPC_STATUS_UNIMPLEMENTED:
          // case GRPC_STATUS_UNKNOWN: // disabled due to dapr {GRPC_STATUS_UNKNOWN; ErrorMsg: 'timeout waiting for address for app id vehicledatabroker'}
            std::cerr << ">>> Unrecoverable error -> stopping broker feeder" << std::endl;
            feeder_active_ = false;
            break;
          default:
            std::cerr << ">>> Maybe temporary error -> trying reconnection to broker" << std::endl;
            break;
        }
        client_->SetDisconnected();
    }
    };

    std::shared_ptr<DataBrokerFeeder> DataBrokerFeeder::createInstance(std::shared_ptr<KuksaClient> client,
                                                                       DatapointConfiguration&& dpConfig) {
        return std::make_shared<DataBrokerFeederImpl>(client, std::move(dpConfig));
    }

}  // namespace broker_feeder
}  // namespace sdv
