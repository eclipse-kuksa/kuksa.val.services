#include "collector_client.h"


namespace sdv {
namespace broker_feeder {

static std::string getEnvVar(const std::string& name, const std::string& defaultValue = {}) {
    char* value = std::getenv(name.c_str());
    return value != nullptr ? std::string(value) : std::string(defaultValue);
}

static GrpcMetadata getGrpcMetadata() {
    GrpcMetadata grpc_metadata;
    std::string dapr_app_id = getEnvVar("VEHICLEDATABROKER_DAPR_APP_ID");
    if (!dapr_app_id.empty()) {
        grpc_metadata["dapr-app-id"] = dapr_app_id;
        std::cout << "setting dapr-app-id: " << dapr_app_id << std::endl;

    }
    return grpc_metadata;
}


std::shared_ptr<CollectorClient> CollectorClient::createInstance(std::string broker_addr) {
    return std::make_shared<CollectorClient>(broker_addr);
}

CollectorClient::CollectorClient(std::string broker_addr)
    : broker_addr_(broker_addr)
    , connected_(false) {
    changeToDaprPortIfSet(broker_addr);
    metadata_ = getGrpcMetadata();
    channel_ = grpc::CreateChannel(broker_addr, grpc::InsecureChannelCredentials());
    stub_ = sdv::databroker::v1::Collector::NewStub(channel_);
}

bool CollectorClient::WaitForConnected(std::chrono::_V2::system_clock::time_point deadline) {
    connected_ = channel_->WaitForConnected(deadline);
    return connected_;
}

grpc_connectivity_state CollectorClient::GetState() { return channel_->GetState(false); }

bool CollectorClient::Connected() { return connected_; }

void CollectorClient::SetDisconnected() { connected_ = false; }

/** Change the port of the broker address passed to the c-tor to the port
 *  set by a possibly set DAPR_GRPC_PORT environment variable. */
void CollectorClient::changeToDaprPortIfSet(std::string& broker_addr) {
    std::string dapr_port = getEnvVar("DAPR_GRPC_PORT");
    if (!dapr_port.empty()) {
        std::string::size_type colon_pos = broker_addr.find_last_of(':');
        broker_addr = broker_addr.substr(0, colon_pos + 1) + dapr_port;
        std::cout << "changing to DAPR GRPC port:" << broker_addr << std::endl;
    }
}

::grpc::Status CollectorClient::RegisterDatapoints(::grpc::ClientContext* context,
                                                   const ::sdv::databroker::v1::RegisterDatapointsRequest& request,
                                                   ::sdv::databroker::v1::RegisterDatapointsReply* response) {
    return stub_->RegisterDatapoints(context, request, response);
}

::grpc::Status CollectorClient::UpdateDatapoints(::grpc::ClientContext* context,
                                                 const ::sdv::databroker::v1::UpdateDatapointsRequest& request,
                                                 ::sdv::databroker::v1::UpdateDatapointsReply* response) {
    return stub_->UpdateDatapoints(context, request, response);
}

std::unique_ptr<::grpc::ClientReader<::sdv::databroker::v1::SubscribeActuatorTargetReply>>
CollectorClient::SubscribeActuatorTargets(::grpc::ClientContext* context,
                                          const ::sdv::databroker::v1::SubscribeActuatorTargetRequest& request) {
    return stub_->SubscribeActuatorTargets(context, request);
}


/** Create the client context for a gRPC call and add possible gRPC metadata */
std::unique_ptr<grpc::ClientContext> CollectorClient::createClientContext()
{
    auto context = std::make_unique<grpc::ClientContext>();
    for (const auto& metadata : metadata_) {
        context->AddMetadata(metadata.first, metadata.second);
    }
    return context;
}


}  // namespace broker_feeder
}  // namespace sdv