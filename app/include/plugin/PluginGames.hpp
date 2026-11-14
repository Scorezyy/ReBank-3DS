#pragma once

#include "i18n/TextId.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

enum class PluginKind { LiveBox, Acnl };

struct PluginGame {
    TextId name;
    PluginKind kind;
    std::span<const std::uint64_t> titleIds;
};

namespace PluginGames {
std::span<const PluginGame> all();
std::string_view catalogId(PluginKind kind);
std::string_view fileName(PluginKind kind);
std::string folderName(std::uint64_t titleId);
std::optional<std::string> installedVersion(const PluginGame& game);
}
