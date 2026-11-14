#include "network/ApiReply.hpp"

namespace {
constexpr const char* InvalidResponse = "The server returned an invalid response.";

bool successStatus(std::uint32_t status) {
    return status >= 200 && status < 300;
}
}

ApiReply::ApiReply(const HttpResponse& response, const char* rejectedMessage)
    : status_(response.status), delivered_(response.success) {
    if (!response.success) {
        error_ = response.message;
        failure_ = response.failureOutcome();
        return;
    }
    body_ = Json::Document::parse(response.body);
    if (!successStatus(status_)) {
        const std::string message = body_.string("message");
        error_ = message.empty() ? rejectedMessage : message;
        const bool serverFault = body_.string("code").empty() && status_ >= 500;
        failure_ = serverFault ? RequestOutcome::Unknown : RequestOutcome::Refused;
        return;
    }
    if (!body_.isObject()) {
        error_ = InvalidResponse;
        failure_ = RequestOutcome::Unknown;
    }
}
