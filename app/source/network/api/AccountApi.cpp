#include "network/api/AccountApi.hpp"

#include "network/ApiReply.hpp"
#include "network/Json.hpp"

#include <utility>

namespace {
constexpr std::size_t DeviceTokenLength = 43;

constexpr std::pair<std::string_view, PolicyBlock::Kind> PolicyKinds[] = {
    {"title", PolicyBlock::Kind::Title},
    {"heading", PolicyBlock::Kind::Heading},
    {"subheading", PolicyBlock::Kind::Subheading},
    {"paragraph", PolicyBlock::Kind::Paragraph},
    {"bullet", PolicyBlock::Kind::Bullet},
    {"row", PolicyBlock::Kind::Row},
};

std::optional<PolicyBlock::Kind> policyKind(std::string_view name) {
    for (const auto& [key, kind] : PolicyKinds) {
        if (key == name) {
            return kind;
        }
    }
    return std::nullopt;
}

std::optional<PolicyBlock> parsePolicyBlock(json_t* entry) {
    const std::optional<PolicyBlock::Kind> kind = policyKind(Json::string(entry, "kind"));
    if (!kind) {
        return std::nullopt;
    }
    PolicyBlock block{*kind, Json::string(entry, "text"), {}};
    json_t* cells = json_object_get(entry, "cells");
    for (std::size_t index = 0; index < json_array_size(cells); ++index) {
        if (const char* value = json_string_value(json_array_get(cells, index))) {
            block.cells.emplace_back(value);
        }
    }
    return block;
}

AuthResult parseSession(const ApiReply& reply) {
    const int status = static_cast<int>(reply.status());
    if (!reply.ok()) {
        return {false, reply.error(), {}, false, status};
    }
    if (reply.status() == 202) {
        return {true, "Password reset request accepted.", {}, false, status};
    }
    const Json::Document& body = reply.body();
    json_t* account = body.field("account");
    AccountSession session{Json::string(account, "id"), body.string("accessToken"), body.string("refreshToken"),
                           Json::integer<std::uint16_t>(account, "boxLimit", 50)};
    if (session.accountId.empty() || session.accessToken.empty() || session.refreshToken.empty()) {
        return {false, "The server returned an incomplete session.", {}, false, status};
    }
    return {true, "Authentication succeeded.", std::move(session), false, status};
}
}

AuthResult AccountApi::login(const std::string& username, const std::string& password) {
    return authenticate("/v1/auth/login", Json::encode({{"username", json_string(username.c_str())},
                                                       {"password", json_string(password.c_str())}}));
}

AuthResult AccountApi::registerAccount(const std::string& username, const std::string& email,
                                       const std::string& password, const std::string& deviceFingerprint) {
    if (deviceFingerprint.empty()) {
        return {false, "This console's identity could not be verified. Registration requires a genuine, unmodified 3DS.",
                {}, false, 0};
    }
    return authenticate("/v1/auth/register", Json::encode({{"username", json_string(username.c_str())},
                                                          {"email", json_string(email.c_str())},
                                                          {"password", json_string(password.c_str())},
                                                          {"deviceFingerprint", json_string(deviceFingerprint.c_str())},
                                                          {"privacyAccepted", json_true()},
                                                          {"ageConfirmed", json_true()}}));
}

AuthResult AccountApi::refresh(const std::string& refreshToken) {
    return authenticate("/v1/auth/refresh", Json::encode({{"refreshToken", json_string(refreshToken.c_str())}}));
}

AuthResult AccountApi::requestPasswordReset(const std::string& email) {
    return authenticate("/v1/auth/password-reset", Json::encode({{"email", json_string(email.c_str())}}));
}

AuthResult AccountApi::authenticate(const std::string& path, const std::string& body) {
    const HttpResponse response = http_.send(HttpMethod::Post, path, body);
    if (!response.success) {
        return {false, response.message, {}, true};
    }
    return parseSession(ApiReply(response, "The server rejected the request."));
}

std::optional<std::string> AccountApi::issueDeviceToken(const std::string& fingerprint, const std::string& accessToken) {
    if (fingerprint.empty() || accessToken.empty()) {
        return std::nullopt;
    }
    const std::string body = Json::encode({{"fingerprint", json_string(fingerprint.c_str())}});
    const ApiReply reply(http_.send(HttpMethod::Post, "/v1/livebox/device-token", body, accessToken),
                         "Device token rejected.");
    const std::string token = reply.ok() ? reply.body().string("deviceToken") : std::string{};
    if (token.size() != DeviceTokenLength) {
        return std::nullopt;
    }
    return token;
}

std::optional<std::vector<PolicyBlock>> AccountApi::privacyPolicy(std::string_view language) {
    const ApiReply reply(http_.send(HttpMethod::Get, "/v1/legal/privacy?lang=" + std::string(language)),
                         "Privacy policy unavailable.");
    if (!reply.ok()) {
        return std::nullopt;
    }
    std::vector<PolicyBlock> blocks;
    json_t* entries = reply.body().field("blocks");
    for (std::size_t index = 0; index < json_array_size(entries); ++index) {
        if (std::optional<PolicyBlock> block = parsePolicyBlock(json_array_get(entries, index))) {
            blocks.push_back(std::move(*block));
        }
    }
    if (blocks.empty()) {
        return std::nullopt;
    }
    return blocks;
}
