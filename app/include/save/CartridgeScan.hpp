#pragma once

#include "gui/icons/IconPixels.hpp"
#include "save/adapter/SaveAdapter.hpp"
#include "save/catalog/GameCatalog.hpp"

#include <cstddef>
#include <optional>
#include <span>

struct IdentifiedCartridge {
    std::size_t catalogIndex = 0;
    bool nintendoDs = false;
    std::optional<SaveSummary> summary;
};

namespace CartridgeScan {
bool inserted();
bool waitForPower();
void resetSlotPower();
std::optional<IdentifiedCartridge> identify(std::span<const GameDescriptor> games);
bool readStableIcon(const GameDescriptor& game, IconPixels& pixels);
bool readStableSummary(const GameDescriptor& game, SaveSummary& summary);
}
