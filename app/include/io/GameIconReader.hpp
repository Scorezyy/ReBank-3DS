#pragma once

#include "save/catalog/GameCatalog.hpp"

#include <array>
#include <cstdint>

namespace GameIconReader {
bool read(const GameDescriptor& game, bool cartridge,
          std::array<std::uint16_t, 48 * 48>& pixels);
}
