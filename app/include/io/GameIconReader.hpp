#pragma once

#include "save/catalog/GameCatalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t GameIconSize = 48;
using IconPixels = std::array<std::uint16_t, GameIconSize * GameIconSize>;

namespace GameIconReader {
bool read(const GameDescriptor& game, bool cartridge, IconPixels& pixels);
bool readUncached(const GameDescriptor& game, bool cartridge, IconPixels& pixels);
}
