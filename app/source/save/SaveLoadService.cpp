#include "save/SaveLoadService.hpp"

#include "gui/icons/GameIconReader.hpp"
#include "platform/Clock.hpp"
#include "platform/Logger.hpp"
#include "save/CartridgeScan.hpp"
#include "save/catalog/GameCatalog.hpp"
#include "save/catalog/VirtualConsoleTitles.hpp"
#include "save/storage/RomLibrary.hpp"

#include <3ds.h>

#include <algorithm>
#include <iterator>
#include <new>

namespace {
constexpr int RescanAttempts = 15;
constexpr s64 RescanSettleNanoseconds = 700'000'000LL;
constexpr s64 RescanRetryNanoseconds = 80'000'000LL;

DiscoveredGame cartridgeEntry(const IdentifiedCartridge& found) {
    DiscoveredGame game;
    game.catalogIndex = found.catalogIndex;
    game.cartridge = true;
    if (found.summary) {
        game.save = *found.summary;
        game.summaryKnown = true;
    }
    return game;
}

template <typename Read>
std::shared_ptr<IconPixels> loadIcon(Read read) {
    std::shared_ptr<IconPixels> icon(new (std::nothrow) IconPixels());
    if (!icon || !read(*icon)) {
        return nullptr;
    }
    return icon;
}
}

void SaveLoadService::begin(Operation operation) {
    if (!claim(operation)) {
        return;
    }
    switch (operation) {
        case Operation::DiscoverGames:
            discoveredGames.clear();
            setPhase(Phase::SearchingGames);
            break;
        case Operation::RescanCartridge:
            setPhase(Phase::SearchingGames);
            break;
        case Operation::Summary:
            summaryResult = {};
            break;
        case Operation::OpenGame:
            openGameResult = {};
            setPhase(Phase::ReadingSave);
            break;
        case Operation::None:
            break;
    }
    launch([this, operation]() { work(operation); });
}

void SaveLoadService::beginDiscovery(bool rebuildLibrary) {
    if (busy()) {
        return;
    }
    rebuildLibrary_ = rebuildLibrary;
    begin(Operation::DiscoverGames);
}

bool SaveLoadService::beginFor(std::size_t index, Operation operation) {
    catalogIndex = index;
    begin(operation);
    return busy();
}

bool SaveLoadService::beginSummary(std::size_t index, bool cartridge) {
    if (busy()) {
        return false;
    }
    summaryCartridge = cartridge;
    return beginFor(index, Operation::Summary);
}

bool SaveLoadService::beginOpen(std::size_t index, SaveSource::Preference preference) {
    if (busy()) {
        return false;
    }
    openSourcePreference = preference;
    return beginFor(index, Operation::OpenGame);
}

void SaveLoadService::work(Operation operation) {
    try {
        switch (operation) {
            case Operation::DiscoverGames:
                discoverGames();
                break;
            case Operation::RescanCartridge:
                rescanCartridge();
                break;
            case Operation::Summary:
                fetchSummary();
                break;
            case Operation::OpenGame:
                openGame();
                break;
            case Operation::None:
                break;
        }
    } catch (...) {
        if (operation == Operation::OpenGame) {
            openGameResult.success = false;
            openGameResult.message = "Loading failed unexpectedly.";
        }
        Logger::instance().error("Unhandled save loading worker exception");
    }
}

std::size_t SaveLoadService::addCartridgeGame() {
    const auto games = supportedGames();
    if (!CartridgeScan::inserted()) {
        return games.size();
    }
    CartridgeScan::waitForPower();
    const std::optional<IdentifiedCartridge> found = CartridgeScan::identify(games);
    if (!found) {
        return games.size();
    }
    discoveredGames.push_back(cartridgeEntry(*found));
    return found->catalogIndex;
}

void SaveLoadService::addStorageGames(std::size_t cartridgeCatalogIndex) {
    const auto games = supportedGames();
    for (std::size_t index = 0; index < games.size(); ++index) {
        if (SaveSource::existsInStorage(games[index])) {
            DiscoveredGame game;
            game.catalogIndex = index;
            game.storageOnly = index == cartridgeCatalogIndex;
            discoveredGames.push_back(std::move(game));
        }
        setProgress(15 + static_cast<int>((index + 1) * 35 / games.size()));
    }
}

void SaveLoadService::readDiscoveredIcons() {
    const auto games = supportedGames();
    const std::size_t iconCount = discoveredGames.size();
    Logger::instance().info("Icon scan starting for " + std::to_string(iconCount) + " games");
    for (std::size_t index = 0; index < iconCount; ++index) {
        DiscoveredGame& game = discoveredGames[index];
        const GameDescriptor& descriptor = games[game.catalogIndex];
        const bool cartridge = game.cartridge;
        game.iconPixels = loadIcon([&](IconPixels& pixels) {
            return cartridge ? GameIconReader::readUncached(descriptor, true, pixels)
                             : GameIconReader::read(descriptor, false, pixels);
        });
        setProgress(static_cast<int>(50 + (index + 1) * 50 / iconCount));
    }
    Logger::instance().info("Icon scan finished");
}

void SaveLoadService::loadRomLibrary() {
    if (rebuildLibrary_ || !RomLibrary::libraryCached()) {
        setPhase(Phase::ScanningLibrary);
    }
    RomLibrary::load(rebuildLibrary_);
    rebuildLibrary_ = false;
    setPhase(Phase::SearchingGames);
}

void SaveLoadService::discoverGames() {
    VirtualConsoleTitles::resetInstalledCache();
    loadRomLibrary();
    const std::size_t cartridgeCatalogIndex = addCartridgeGame();
    setProgress(15);
    addStorageGames(cartridgeCatalogIndex);
    setPhase(Phase::ReadingIcons);
    readDiscoveredIcons();
    digitalCache_.clear();
    std::copy_if(discoveredGames.begin(), discoveredGames.end(), std::back_inserter(digitalCache_),
                 [](const DiscoveredGame& game) { return !game.cartridge; });
}

void SaveLoadService::rescanCartridge() {
    const auto games = supportedGames();
    discoveredGames = digitalCache_;

    const u64 rescanStart = Clock::now();
    CartridgeScan::resetSlotPower();
    svcSleepThread(RescanSettleNanoseconds);

    for (int attempt = 0; attempt < RescanAttempts; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(RescanRetryNanoseconds);
        }
        setProgress(static_cast<int>(10 + attempt * 6));

        const std::optional<IdentifiedCartridge> found = CartridgeScan::identify(games);
        if (!found) {
            continue;
        }
        DiscoveredGame game = cartridgeEntry(*found);
        game.iconPixels = loadIcon([&](IconPixels& pixels) {
            return CartridgeScan::readStableIcon(games[game.catalogIndex], pixels);
        });
        if (!game.iconPixels && found->nintendoDs) {
            continue;
        }
        Logger::instance().info("RescanCartridge: identified " + std::string(games[game.catalogIndex].code)
            + " after " + std::to_string(Clock::millisecondsSince(rescanStart)) + "ms");
        discoveredGames.push_back(std::move(game));
        setProgress(100);
        return;
    }
    setProgress(100);
}

void SaveLoadService::fetchSummary() {
    const auto games = supportedGames();
    if (catalogIndex >= games.size()) {
        return;
    }
    SaveSummary summary;
    if (summaryCartridge) {
        CartridgeScan::readStableSummary(games[catalogIndex], summary);
    } else {
        SaveAdapter candidate;
        std::string error;
        if (candidate.open(games[catalogIndex], error, SaveSource::Preference::StorageOnly)) {
            summary = candidate.summary();
        }
    }
    summaryResult = summary;
}

void SaveLoadService::openGame() {
    const auto games = supportedGames();
    if (catalogIndex >= games.size()) {
        openGameResult.message = "Invalid game selection.";
        return;
    }
    if (!saveAdapter_.open(games[catalogIndex], openGameResult.message, openSourcePreference)) {
        return;
    }

    openGameResult.save = saveAdapter_.summary();
    openGameResult.localBox = saveAdapter_.currentBox();
    openGameResult.localBoxName = saveAdapter_.boxName(openGameResult.localBox);
    setProgress(10);
    setPhase(Phase::SearchingPokemon);

    openGameResult.localPokemon = saveAdapter_.readBox(openGameResult.localBox);
    setProgress(85);
    openGameResult.localParty = saveAdapter_.readParty();

    setProgress(100);
    openGameResult.success = true;
}
