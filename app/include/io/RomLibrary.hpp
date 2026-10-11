#pragma once

#include "io/GameIconReader.hpp"
#include "save/catalog/GameCatalog.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct RomSave {
    std::string romPath;
    std::string savePath;
    std::uint32_t bannerOffset = 0;
};

namespace RomLibrary {
bool supports(GamePlatform platform);
bool libraryCached();
void load(bool rebuild);
std::optional<RomSave> find(std::string_view code);
bool readIcon(const RomSave& rom, IconPixels& pixels);
}
