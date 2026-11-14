#pragma once

#include "network/ApiTypes.hpp"
#include "network/HttpClient.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class AccountApi {
public:
    explicit AccountApi(HttpClient& http) : http_(http) {}

    AuthResult login(const std::string& username, const std::string& password);
    AuthResult registerAccount(const std::string& username, const std::string& email, const std::string& password,
                               const std::string& deviceFingerprint);
    AuthResult refresh(const std::string& refreshToken);
    AuthResult requestPasswordReset(const std::string& email);
    std::optional<std::string> issueDeviceToken(const std::string& fingerprint, const std::string& accessToken);
    std::optional<std::vector<PolicyBlock>> privacyPolicy(std::string_view language);

private:
    AuthResult authenticate(const std::string& path, const std::string& body);

    HttpClient& http_;
};
