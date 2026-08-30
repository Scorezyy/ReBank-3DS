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
    
    bool poll(Completed& completed);
    bool isRunning() const { return task_.running(); }

private:
    AsyncTask<Completed> task_;
};
