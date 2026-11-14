#pragma once

#include "network/ApiTypes.hpp"
#include "network/HttpClient.hpp"

#include <atomic>
#include <cstdint>
#include <string>

class ReleaseApi {
public:
    explicit ReleaseApi(HttpClient& http) : http_(http) {}

    ClientUpdate latestClientUpdate();
    FileDownloadResult downloadClientUpdate(const std::string& tag, const std::string& assetName,
                                            const std::string& destination, std::uint32_t expectedSize,
                                            std::atomic<std::uint32_t>& received);
    PluginCatalog pluginCatalog(const std::string& accessToken);
    FileDownloadResult downloadPlugin(const std::string& id, const std::string& tag, const std::string& destination,
                                      std::uint32_t expectedSize, std::atomic<std::uint32_t>& received,
                                      const std::string& accessToken);

private:
    HttpClient& http_;
};
