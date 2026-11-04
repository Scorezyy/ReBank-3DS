#include "account/AuthController.hpp"

#include "core/AppPaths.hpp"
#include "core/Logger.hpp"

#include <cstdio>
#include <utility>

namespace {

constexpr std::string_view PluginFolder = "LivePlugin";
constexpr std::string_view DeviceTokenFile = "LivePlugin/device.token";

}

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
    const bool signedIn = request.operation == AuthOperation::Login || request.operation == AuthOperation::Register;
    if (signedIn && completed.result.success) {
        storeDeviceToken(request.deviceFingerprint, completed.result.session.accessToken);
    }
    return completed;
}

void AuthController::storeDeviceToken(const std::string& fingerprint, const std::string& accessToken) {
    const std::optional<std::string> token = api_.issueDeviceToken(fingerprint, accessToken);
    if (!token) {
        Logger::instance().warning("LiveBox device token could not be issued");
        return;
    }
    AppPaths::ensureDirectory(PluginFolder);
    const std::string path = AppPaths::file(DeviceTokenFile);
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        Logger::instance().error("LiveBox device token could not be saved");
        return;
    }
    std::fwrite(token->data(), 1, token->size(), file);
    std::fclose(file);
    Logger::instance().info("LiveBox device token saved");
}
