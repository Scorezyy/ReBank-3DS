#include "account/AuthController.hpp"

#include <utility>

bool AuthController::begin(AuthRequest request) {
    return task_.start([this, request = std::move(request)]() { return run(request); });
}

AuthController::Completed AuthController::run(const AuthRequest& request) {
    Completed completed{request.operation, request.username, {}};
    switch (request.operation) {
        case AuthOperation::Register:
            completed.result = api_.registerAccount(request.username, request.email, request.password,
                                                    request.deviceFingerprint);
            break;
        case AuthOperation::ResetPassword:
            completed.result = api_.requestPasswordReset(request.email);
            break;
        case AuthOperation::Refresh:
            completed.result = api_.refresh(request.refreshToken);
            break;
        case AuthOperation::Login:
            completed.result = api_.login(request.username, request.password);
            break;
    }
    return completed;
}
