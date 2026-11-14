#pragma once

#include "gui/icons/IconPixels.hpp"
#include "save/catalog/GameCatalog.hpp"

namespace GameIconReader {
bool read(const GameDescriptor& game, bool cartridge, IconPixels& pixels);
bool readUncached(const GameDescriptor& game, bool cartridge, IconPixels& pixels);
}
