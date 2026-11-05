#pragma once

#include "network/ApiClient.hpp"
#include "update/PluginLoader.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct PluginInstallProgress {
    std::atomic<std::uint32_t> received{0};
    std::atomic<std::uint32_t> total{0};
};

enum class PluginInstallOutcome { Installed, Unreachable, Rejected, StorageFailed };

struct PluginInstallJob {
    std::string id;
    std::string tag;
    std::string fileName;
    UpdateAsset file;
    std::vector<std::string_view> folders;
};

namespace PluginInstaller {
PluginInstallOutcome install(ApiClient& api, const std::string& accessToken, const PluginInstallJob& job, PluginInstallProgress& progress);
}
