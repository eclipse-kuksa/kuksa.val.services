#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <memory>

#include "sdv/databroker/v1/collector.grpc.pb.h"
#include "kuksa/val/v1/val.grpc.pb.h"

namespace sdv {
namespace broker_feeder {

using GrpcMetadata = std::map<std::string, std::string>;


class CollectorClient {
   public:

    /**
     * Create a new instance
     *
     * @param broker_addr address of the broker to connect to; format "<ip-address>:<port>"
     */
    static std::shared_ptr<CollectorClient> createInstance(std::string broker_addr);

    CollectorClient(std::string broker_addr);

    bool WaitForConnected(std::chrono::_V2::system_clock::time_point deadline);

    grpc_connectivity_state GetState();
    bool Connected();

    void SetDisconnected();

    /** Change the port of the broker address passed to the c-tor to the port
     *  set by a possibly set DAPR_GRPC_PORT environment variable. */
    void changeToDaprPortIfSet(std::string& broker_addr);

    ::grpc::Status RegisterDatapoints(::grpc::ClientContext* context,
                                      const ::sdv::databroker::v1::RegisterDatapointsRequest& request,
                                      ::sdv::databroker::v1::RegisterDatapointsReply* response);

    ::grpc::Status UpdateDatapoints(::grpc::ClientContext* context,
                                    const ::sdv::databroker::v1::UpdateDatapointsRequest& request,
                                    ::sdv::databroker::v1::UpdateDatapointsReply* response);

    std::unique_ptr<::grpc::ClientReader<::kuksa::val::v1::SubscribeResponse>> Subscribe(
        ::grpc::ClientContext* context, const ::kuksa::val::v1::SubscribeRequest& request);

    std::unique_ptr<grpc::ClientContext> createClientContext();

   private:

    GrpcMetadata metadata_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<sdv::databroker::v1::Collector::Stub> stub_;
    std::unique_ptr<kuksa::val::v1::VAL::Stub> kuksa_stub_;

    std::atomic<bool> connected_;

    std::string broker_addr_;
};



}
}
