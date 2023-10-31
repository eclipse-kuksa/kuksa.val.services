#include "kuksa_client.h"

#include <sstream>

namespace sdv {

namespace utils {

using ::grpc::Status;
using ::grpc::StatusCode;

std::string toString(::grpc::StatusCode code) {
    switch (code) {
        case StatusCode::OK:
            return "OK";
        case StatusCode::CANCELLED:
            return "CANCELLED";
        case StatusCode::UNKNOWN:
            return "UNKNOWN";
        case StatusCode::INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case StatusCode::DEADLINE_EXCEEDED:
            return "DEADLINE_EXCEEDED";
        case StatusCode::NOT_FOUND:
            return "NOT_FOUND";
        case StatusCode::ALREADY_EXISTS:
            return "ALREADY_EXISTS";
        case StatusCode::PERMISSION_DENIED:
            return "PERMISSION_DENIED";
        case StatusCode::UNAUTHENTICATED:
            return "UNAUTHENTICATED";
        case StatusCode::RESOURCE_EXHAUSTED:
            return "RESOURCE_EXHAUSTED";
        case StatusCode::FAILED_PRECONDITION:
            return "FAILED_PRECONDITION";
        case StatusCode::ABORTED:
            return "ABORTED";
        case StatusCode::OUT_OF_RANGE:
            return "OUT_OF_RANGE";
        case StatusCode::UNIMPLEMENTED:
            return "UNIMPLEMENTED";
        case StatusCode::INTERNAL:
            return "INTERNAL";
        case StatusCode::UNAVAILABLE:
            return "UNAVAILABLE";
        case StatusCode::DATA_LOSS:
            return "DATA_LOSS";
        default:
            return "<Invalid:" + std::to_string(code) + ">";
    }
}

std::string toString(::grpc_connectivity_state state) {
    switch (state) {
        case GRPC_CHANNEL_IDLE:
            return "IDLE";
        case GRPC_CHANNEL_CONNECTING:
            return "CONNECTING";
        case GRPC_CHANNEL_READY:
            return "READY";
        case GRPC_CHANNEL_TRANSIENT_FAILURE:
            return "TRANSIENT_FAILURE";
        case GRPC_CHANNEL_SHUTDOWN:
            return "SHUTDOWN";
    }
    return "<InvalidState>";
}

std::string toString(::grpc::Status status) {
    std::ostringstream os;
    os << "grpcStatus{"
       << sdv::utils::toString(status.error_code());
    if (status.error_code() != StatusCode::OK) {
       os << ", code:" << status.error_code();
    }
    if (!status.error_message().empty()) {
        os << ", '" << status.error_message() << "'";
    }
    if (!status.error_details().empty()) {
        os << ", details:'" << status.error_details() << "'";
    }
    os << "}";
    return os.str();
}

std::string getEnvVar(const std::string& name, const std::string& defaultValue) {
    char * value = std::getenv(name.c_str());
    return value != nullptr? std::string(value) : std::string(defaultValue);
}

} // namespace utils

namespace broker_feeder {

static GrpcMetadata getGrpcMetadata() {
    GrpcMetadata grpc_metadata;
    std::string dapr_app_id = sdv::utils::getEnvVar("VEHICLEDATABROKER_DAPR_APP_ID");
    if (!dapr_app_id.empty()) {
        grpc_metadata["dapr-app-id"] = dapr_app_id;
        std::cout << "setting dapr-app-id: " << dapr_app_id << std::endl;

    }
    return grpc_metadata;
}


std::shared_ptr<KuksaClient> KuksaClient::createInstance(std::string broker_addr) {
    return std::make_shared<KuksaClient>(broker_addr);
}

KuksaClient::KuksaClient(std::string broker_addr)
    : broker_addr_(broker_addr)
    , connected_(false) {
    changeToDaprPortIfSet(broker_addr);
    metadata_ = getGrpcMetadata();
    channel_ = grpc::CreateChannel(broker_addr, grpc::InsecureChannelCredentials());
    stub_ = sdv::databroker::v1::Collector::NewStub(channel_);
    kuksa_stub_ = kuksa::val::v1::VAL::NewStub(channel_);
    broker_stub_ = sdv::databroker::v1::Broker::NewStub(channel_);
}

bool KuksaClient::WaitForConnected(std::chrono::_V2::system_clock::time_point deadline) {
    connected_ = channel_->WaitForConnected(deadline);
    return connected_;
}

grpc_connectivity_state KuksaClient::GetState() { return channel_->GetState(false); }

bool KuksaClient::Connected() { return connected_; }

void KuksaClient::SetDisconnected() { connected_ = false; }

/** Change the port of the broker address passed to the c-tor to the port
 *  set by a possibly set DAPR_GRPC_PORT environment variable. */
void KuksaClient::changeToDaprPortIfSet(std::string& broker_addr) {
    std::string dapr_port = sdv::utils::getEnvVar("DAPR_GRPC_PORT");
    if (!dapr_port.empty()) {
        std::string::size_type colon_pos = broker_addr.find_last_of(':');
        broker_addr = broker_addr.substr(0, colon_pos + 1) + dapr_port;
        std::cout << "changing to DAPR GRPC port:" << broker_addr << std::endl;
    }
}

::grpc::Status KuksaClient::RegisterDatapoints(::grpc::ClientContext* context,
                                                   const ::sdv::databroker::v1::RegisterDatapointsRequest& request,
                                                   ::sdv::databroker::v1::RegisterDatapointsReply* response) {
    return stub_->RegisterDatapoints(context, request, response);
}

::grpc::Status KuksaClient::UpdateDatapoints(::grpc::ClientContext* context,
                                                 const ::sdv::databroker::v1::UpdateDatapointsRequest& request,
                                                 ::sdv::databroker::v1::UpdateDatapointsReply* response) {
    return stub_->UpdateDatapoints(context, request, response);
}

std::unique_ptr<::grpc::ClientReader<::kuksa::val::v1::SubscribeResponse>>
KuksaClient::Subscribe(::grpc::ClientContext *context,
                           const ::kuksa::val::v1::SubscribeRequest &request) {
    return kuksa_stub_->Subscribe(context, request);
}

::grpc::Status KuksaClient::GetMetadata(::grpc::ClientContext* context,
                                            const ::sdv::databroker::v1::GetMetadataRequest& request,
                                            ::sdv::databroker::v1::GetMetadataReply* response) {

    return broker_stub_->GetMetadata(context, request, response);
}

/** Create the client context for a gRPC call and add possible gRPC metadata */
std::unique_ptr<grpc::ClientContext> KuksaClient::createClientContext()
{
    auto context = std::make_unique<grpc::ClientContext>();
    for (const auto& metadata : metadata_) {
        context->AddMetadata(metadata.first, metadata.second);
    }
    return context;
}

}  // namespace broker_feeder
}  // namespace sdv