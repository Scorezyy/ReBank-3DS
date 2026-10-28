#pragma once

#include "network/ApiClient.hpp"

#include <atomic>
#include <cstdint>
#include <string>

struct UpdateInstallResult {
    bool success = false;
    bool updated = false;
    std::string message;
    bool keptForManualInstall = false;
};

enum class UpdateStage { Checking, Downloading, Verifying, Installing, Installed, Failed };

struct UpdateProgress {
    std::atomic<UpdateStage> stage{UpdateStage::Checking};
    std::atomic<std::uint32_t> received{0};
    std::atomic<std::uint32_t> total{0};
    std::string version;
};

namespace UpdateInstaller {
UpdateInstallResult run(ApiClient& api, const std::string& executablePath, bool homebrew, UpdateProgress& progress);
}
