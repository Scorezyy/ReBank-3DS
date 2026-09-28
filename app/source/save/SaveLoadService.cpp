#include "save/SaveLoadService.hpp"
#include "core/Logger.hpp"
#include "save/catalog/GameCatalog.hpp"
#include "save/catalog/VirtualConsoleTitles.hpp"
#include "io/SaveMedium.hpp"

#include <3ds.h>

#include <algorithm>
#include <iterator>
#include <new>

namespace {
bool waitForCardPower() {
    for (int attempt = 0; attempt < 40; ++attempt) {
        bool powered = false;
        if (R_SUCCEEDED(FSUSER_CardSlotGetCardIFPowerStatus(&powered)) && powered) {
            return true;
        }
        svcSleepThread(25'000'000LL);
    }
    return false;
}

void resetCardSlotPower() {
    bool powerStatus = false;
    FSUSER_CardSlotPowerOff(&powerStatus);
    FSUSER_CardSlotPowerOn(&powerStatus);
    waitForCardPower();
}

std::shared_ptr<IconPixels> allocateIcon() {
    return std::shared_ptr<IconPixels>(new (std::nothrow) IconPixels());
}

bool readStableCartridgeIcon(const GameDescriptor& game, IconPixels& pixels) {
    IconPixels previous{};
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(120'000'000LL);
        }
        if (!GameIconReader::readUncached(game, true, pixels)) {
            continue;
        }
        if (attempt > 0 && previous == pixels) {
            return true;
        }
        previous = pixels;
    }
    return false;
}

bool summaryLooksValid(const SaveSummary& summary) {
    return !summary.trainerName.empty() || summary.trainerId != 0 || summary.playTimeMinutes != 0;
}

bool openCartridgeSummaryStable(const GameDescriptor& game, SaveSummary& outSummary) {
    bool opened = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(100'000'000LL);
        }
        SaveAdapter candidate;
        std::string error;
        if (!candidate.open(game, error, SaveAdapter::SourcePreference::CartridgeOnly)) {
            continue;
        }
        opened = true;
        outSummary = candidate.summary();
        if (summaryLooksValid(outSummary)) {
            return true;
        }
    }
    return opened;
}

bool identifyDsCartridge(std::span<const GameDescriptor> games, DiscoveredGame& game) {
    const std::string code = SaveMedium::dsGameCodeFromHeader();
    if (code.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < games.size(); ++index) {
        if (games[index].platform == GamePlatform::NintendoDs && games[index].code == code) {
            game.catalogIndex = index;
            game.cartridge = true;
            return true;
        }
    }
    return false;
}

bool identify3dsCartridge(std::span<const GameDescriptor> games, DiscoveredGame& game) {
    const std::optional<std::string_view> code = SaveMedium::codeFor(SaveMedium::cartridgeTitleId());
    if (!code) {
        return false;
    }
    const auto match = std::find_if(games.begin(), games.end(), [&](const GameDescriptor& candidate) {
        return candidate.platform == GamePlatform::Nintendo3Ds && candidate.code == *code;
    });
    if (match == games.end()) {
        return false;
    }
    SaveSummary summary;
    if (!openCartridgeSummaryStable(*match, summary)) {
        return false;
    }
    game.catalogIndex = static_cast<std::size_t>(match - games.begin());
    game.save = summary;
    game.summaryKnown = true;
    game.cartridge = true;
    return true;
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

bool SaveLoadService::beginOpen(std::size_t index, SaveAdapter::SourcePreference preference) {
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
    bool inserted = false;
    FSUSER_CardSlotIsInserted(&inserted);
    if (!inserted) {
        return games.size();
    }
    waitForCardPower();
    DiscoveredGame game;
    if (!identifyDsCartridge(games, game) && !identify3dsCartridge(games, game)) {
        return games.size();
    }
    discoveredGames.push_back(game);
    return game.catalogIndex;
}

void SaveLoadService::addStorageGames(std::size_t cartridgeCatalogIndex) {
    const auto games = supportedGames();
    for (std::size_t index = 0; index < games.size(); ++index) {
        if (SaveAdapter::storageSaveExists(games[index])) {
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
        game.iconPixels = allocateIcon();
        const bool iconRead = game.iconPixels && (game.cartridge
            ? GameIconReader::readUncached(descriptor, true, *game.iconPixels)
            : GameIconReader::read(descriptor, false, *game.iconPixels));
        if (!iconRead) {
            game.iconPixels.reset();
        }
        setProgress(static_cast<int>(50 + (index + 1) * 50 / iconCount));
    }
    Logger::instance().info("Icon scan finished");
}

void SaveLoadService::discoverGames() {
    VirtualConsoleTitles::resetInstalledCache();
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

    const u64 rescanStart = svcGetSystemTick();
    resetCardSlotPower();
    svcSleepThread(700'000'000LL);

    for (int attempt = 0; attempt < 15; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(80'000'000LL);
        }
        setProgress(static_cast<int>(10 + attempt * 6));

        DiscoveredGame game;
        const bool identifiedAsDs = identifyDsCartridge(games, game);
        const bool identified = identifiedAsDs || identify3dsCartridge(games, game);
        if (!identified) {
            continue;
        }

        game.iconPixels = allocateIcon();
        const bool iconOk = game.iconPixels && readStableCartridgeIcon(games[game.catalogIndex], *game.iconPixels);
        if (!iconOk) {
            game.iconPixels.reset();
            if (identifiedAsDs) {
                continue;
            }
        }
        Logger::instance().info("RescanCartridge: identified " + std::string(games[game.catalogIndex].code)
            + " after " + std::to_string((svcGetSystemTick() - rescanStart) * 1000 / SYSCLOCK_ARM11) + "ms");
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
        openCartridgeSummaryStable(games[catalogIndex], summary);
    } else {
        SaveAdapter candidate;
        std::string error;
        if (candidate.open(games[catalogIndex], error, SaveAdapter::SourcePreference::StorageOnly)) {
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
