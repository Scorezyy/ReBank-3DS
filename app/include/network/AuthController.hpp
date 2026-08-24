#pragma once

#include "core/AsyncTask.hpp"
#include "network/ApiClient.hpp"

#include <string>

enum class AuthOperation {
    Login,
    Register,
    ResetPassword,
    Refresh
};

// Runs login/register/password-reset/session-refresh calls on a background
// thread. `password` doubles as the refresh token for AuthOperation::Refresh,
// matching how the server call itself is shaped.
class AuthController {
public:
    struct Completed {
        AuthOperation operation = AuthOperation::Login;
        std::string username;
        std::string email;
        std::string password;
        AuthResult result;
    };

    bool begin(
        ApiClient& api,
        AuthOperation operation,
        std::string username,
        std::string email,
        std::string password
    );
    // Call once per frame. Returns true the moment a finished auth result
    // becomes available.
    bool poll(Completed& completed);
    bool isRunning() const { return task_.running(); }

private:
    AsyncTask<Completed> task_;
};
