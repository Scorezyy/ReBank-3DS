#include "network/AuthController.hpp"

bool AuthController::begin(
    ApiClient& api,
    AuthOperation operation,
    std::string username,
    std::string email,
    std::string password
) {
    ApiClient* apiPtr = &api;
    return task_.start([apiPtr, operation, username = std::move(username),
                         email = std::move(email), password = std::move(password)]() {
        Completed completed;
        completed.operation = operation;
        completed.username = username;
        completed.email = email;
        completed.password = password;
        switch (operation) {
            case AuthOperation::Register:
                completed.result = apiPtr->registerAccount(username, email, password);
                break;
            case AuthOperation::ResetPassword:
                completed.result = apiPtr->requestPasswordReset(email);
                break;
            case AuthOperation::Refresh:
                completed.result = apiPtr->refresh(password);
                break;
            default:
                completed.result = apiPtr->login(username, password);
                break;
        }
        return completed;
    });
}

bool AuthController::poll(Completed& completed) {
    return task_.poll(completed);
}
