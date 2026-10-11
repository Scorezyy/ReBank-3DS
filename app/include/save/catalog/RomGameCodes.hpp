#pragma once

#include "save/catalog/GameCatalog.hpp"

#include <optional>
#include <string_view>

namespace RomGameCodes {
std::optional<std::string_view> catalogCode(GamePlatform platform, std::string_view gameCode);
}
