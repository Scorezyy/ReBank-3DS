#pragma once

#include "save/GameCatalog.hpp"

#include <array>
#include <cstdint>

namespace IconCache {
bool load(const GameDescriptor& game, bool cartridge, std::array<std::uint16_t, 48 * 48>& pixels);
void store(const GameDescriptor& game, bool cartridge, const std::array<std::uint16_t, 48 * 48>& pixels);
}
