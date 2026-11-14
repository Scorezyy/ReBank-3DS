#pragma once

#include "network/HttpClient.hpp"
#include "network/Json.hpp"

#include <string>

class ApiReply {
public:
    ApiReply(const HttpResponse& response, const char* rejectedMessage);

    bool ok() const { return error_.empty(); }
    bool delivered() const { return delivered_; }
    bool accepted() const { return delivered_ && status_ >= 200 && status_ < 300; }
    std::uint32_t status() const { return status_; }
    const Json::Document& body() const { return body_; }
    const std::string& error() const { return error_; }
    RequestOutcome failure() const { return failure_; }

private:
    Json::Document body_;
    std::string error_;
    RequestOutcome failure_ = RequestOutcome::Unknown;
    std::uint32_t status_ = 0;
    bool delivered_ = false;
};
