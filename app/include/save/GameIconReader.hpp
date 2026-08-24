#pragma once

#include "save/GameCatalog.hpp"

#include <array>
#include <cstdint>

// Reads the 48x48 box-art icon for a detected game, straight from the
// console - a DS cartridge banner or a 3DS title's SMDH, whichever the
// game actually uses. Pure pixel decoding, no save-file state involved.
namespace GameIconReader {
bool read(const GameDescriptor& game, bool cartridge,
          std::array<std::uint16_t, 48 * 48>& pixels);
}
