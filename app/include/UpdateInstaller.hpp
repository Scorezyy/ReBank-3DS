#pragma once

#include "ApiClient.hpp"

#include <string>

struct UpdateInstallResult {
    bool success = false;
    bool updated = false;
    std::string message;
};

class UpdateInstaller {
public:
    static UpdateInstallResult run(ApiClient& api, const std::string& executablePath, bool homebrew);
};
