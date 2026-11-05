#pragma once

#include "i18n/Localization.hpp"

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
    std::span<const std::string_view> folders;
};

namespace PluginGames {
std::span<const PluginGame> all();
std::string_view catalogId(PluginKind kind);
std::string_view fileName(PluginKind kind);
std::optional<std::string> installedVersion(const PluginGame& game);
}
