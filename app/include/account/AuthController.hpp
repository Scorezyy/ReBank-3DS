#pragma once

#include "core/AsyncTask.hpp"
#include "network/ApiClient.hpp"

#include <string>
#include <utility>

enum class AuthOperation {
    Login,
    Register,
    ResetPassword,
    Refresh
};

struct AuthRequest {
    AuthOperation operation = AuthOperation::Login;
    std::string username;
    std::string email;
    std::string password;
    std::string refreshToken;
    std::string deviceFingerprint;

    static AuthRequest login(std::string username, std::string password) {
        AuthRequest request;
        request.username = std::move(username);
        request.password = std::move(password);
        return request;
    }

    static AuthRequest registration(std::string username, std::string email, std::string password) {
        AuthRequest request = login(std::move(username), std::move(password));
        request.operation = AuthOperation::Register;
        request.email = std::move(email);
        return request;
    }

    static AuthRequest passwordReset(std::string email) {
        AuthRequest request;
        request.operation = AuthOperation::ResetPassword;
        request.email = std::move(email);
        return request;
    }

    static AuthRequest refresh(std::string refreshToken) {
        AuthRequest request;
        request.operation = AuthOperation::Refresh;
        request.refreshToken = std::move(refreshToken);
        return request;
    }
};

class AuthController {
public:
    struct Completed {
        AuthOperation operation = AuthOperation::Login;
        std::string username;
        AuthResult result;
    };

    explicit AuthController(ApiClient& api) : api_(api) {}

    bool begin(AuthRequest request);
    bool poll(Completed& completed) { return task_.poll(completed); }
    bool isRunning() const { return task_.running(); }

private:
    Completed run(const AuthRequest& request);

    ApiClient& api_;
    AsyncTask<Completed> task_;
};
