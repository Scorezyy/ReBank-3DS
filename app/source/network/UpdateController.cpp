#include "network/UpdateController.hpp"

bool UpdateController::begin(ApiClient& api, std::string executablePath, bool homebrew) {
    ApiClient* apiPtr = &api;
    return task_.start([apiPtr, executablePath = std::move(executablePath), homebrew]() {
        return UpdateInstaller::run(*apiPtr, executablePath, homebrew);
    });
}

bool UpdateController::poll(UpdateInstallResult& result) {
    return task_.poll(result);
}
