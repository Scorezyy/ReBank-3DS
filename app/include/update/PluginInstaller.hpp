#pragma once

#include "network/ApiClient.hpp"
#include "update/PluginLoader.hpp"

#include <atomic>
#include <cstdint>
#include <string>

struct PluginInstallProgress {
    std::atomic<std::uint32_t> received{0};
    std::atomic<std::uint32_t> total{0};
};

enum class PluginInstallOutcome { Installed, Unreachable, Rejected, StorageFailed };

struct PluginInstallResult {
    PluginInstallOutcome outcome = PluginInstallOutcome::Unreachable;
    std::string version;
    PluginLoaderState loader = PluginLoaderState::Manual;
};

namespace PluginInstaller {
PluginInstallResult run(ApiClient& api, PluginInstallProgress& progress);
}
