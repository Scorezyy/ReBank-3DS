#include "network/api/ReleaseApi.hpp"

#include "network/ApiReply.hpp"
#include "network/Json.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>

namespace {
constexpr std::size_t MaximumAssetSize = 5 * 1024 * 1024;
constexpr std::size_t MaximumVersionLength = 48;
constexpr std::string_view CiaAssetName = "ReBank.cia";
constexpr std::string_view ThreeDsxAssetName = "ReBank.3dsx";
constexpr std::string_view PluginIds[] = {"livebox", "acnl"};

bool validVersion(std::string_view value) {
    return !value.empty() && value.size() <= MaximumVersionLength
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isalnum(character) || character == '.' || character == '-';
           });
}

bool validTag(std::string_view value) {
    return value.size() > 1 && value.size() <= MaximumVersionLength && value.front() == 'v' && validVersion(value.substr(1));
}

bool validSha256(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isdigit(character) || (character >= 'a' && character <= 'f');
    });
}

std::optional<UpdateAsset> parseAsset(json_t* asset) {
    UpdateAsset parsed{Json::string(asset, "sha256"), Json::string(asset, "signature"), 0};
    const json_int_t size = Json::integer<json_int_t>(asset, "size", 0);
    if (!validSha256(parsed.sha256) || size <= 0 || size > static_cast<json_int_t>(MaximumAssetSize)) {
        return std::nullopt;
    }
    parsed.size = static_cast<std::uint32_t>(size);
    return parsed;
}

std::optional<CatalogPlugin> parsePlugin(json_t* entry) {
    CatalogPlugin plugin{Json::string(entry, "id"), Json::flag(entry, "access"), Json::flag(entry, "available"),
                         Json::string(entry, "tag"), Json::string(entry, "version"), {}};
    if (plugin.id.empty()) {
        return std::nullopt;
    }
    if (!plugin.available) {
        return plugin;
    }
    std::optional<UpdateAsset> file = parseAsset(entry);
    if (!file || file->signature.empty() || !validTag(plugin.tag) || !validVersion(plugin.version)) {
        plugin.available = false;
        return plugin;
    }
    plugin.file = std::move(*file);
    return plugin;
}

bool knownPlugin(std::string_view id) {
    return std::find(std::begin(PluginIds), std::end(PluginIds), id) != std::end(PluginIds);
}
}

ClientUpdate ReleaseApi::latestClientUpdate() {
    ClientUpdate update;
    const ApiReply reply(http_.send(HttpMethod::Get, "/v1/client/update"), "Update information unavailable.");
    json_t* assets = reply.body().field("assets");
    if (!reply.ok() || reply.status() != 200 || !json_is_array(assets)) {
        update.message = reply.delivered() ? "The server returned invalid update information." : reply.error();
        return update;
    }
    update.tag = reply.body().string("tag");
    update.version = reply.body().string("version");
    for (std::size_t index = 0; index < json_array_size(assets); ++index) {
        json_t* asset = json_array_get(assets, index);
        const std::string name = Json::string(asset, "name");
        std::optional<UpdateAsset> parsed = parseAsset(asset);
        if (parsed && name == CiaAssetName) {
            update.cia = std::move(*parsed);
        } else if (parsed && name == ThreeDsxAssetName) {
            update.threeDsx = std::move(*parsed);
        }
    }
    if (!validTag(update.tag) || !validVersion(update.version) || update.cia.sha256.empty()
        || update.threeDsx.sha256.empty()) {
        update.message = "The server returned incomplete update information.";
        return update;
    }
    update.success = true;
    update.message = "Update information loaded.";
    return update;
}

FileDownloadResult ReleaseApi::downloadClientUpdate(const std::string& tag, const std::string& assetName,
                                                    const std::string& destination, std::uint32_t expectedSize,
                                                    std::atomic<std::uint32_t>& received) {
    if (!validTag(tag) || (assetName != CiaAssetName && assetName != ThreeDsxAssetName)) {
        return {false, "The update download request is invalid.", 0};
    }
    return http_.download("/v1/client/update/" + tag + "/" + assetName, destination, expectedSize, received);
}

PluginCatalog ReleaseApi::pluginCatalog(const std::string& accessToken) {
    PluginCatalog catalog;
    const ApiReply reply(http_.send(HttpMethod::Get, "/v1/plugins", {}, accessToken),
                         "The plugin list could not be loaded.");
    if (!reply.ok() || reply.status() != 200) {
        catalog.message = reply.delivered() ? "The plugin list could not be loaded." : reply.error();
        return catalog;
    }
    json_t* entries = reply.body().field("plugins");
    if (!json_is_array(entries)) {
        catalog.message = "The server returned an invalid plugin list.";
        return catalog;
    }
    for (std::size_t index = 0; index < json_array_size(entries); ++index) {
        if (std::optional<CatalogPlugin> plugin = parsePlugin(json_array_get(entries, index))) {
            catalog.plugins.push_back(std::move(*plugin));
        }
    }
    catalog.success = true;
    catalog.message = "Plugin list loaded.";
    return catalog;
}

FileDownloadResult ReleaseApi::downloadPlugin(const std::string& id, const std::string& tag,
                                              const std::string& destination, std::uint32_t expectedSize,
                                              std::atomic<std::uint32_t>& received, const std::string& accessToken) {
    if (!knownPlugin(id) || !validTag(tag)) {
        return {false, "The plugin download request is invalid.", 0};
    }
    return http_.download("/v1/plugins/download/" + id + "/" + tag, destination, expectedSize, received, accessToken);
}
