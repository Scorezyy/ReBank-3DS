#include "save/SaveLoadService.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "save/catalog/GameCatalog.hpp"
#include "io/GameIconReader.hpp"
#include "io/IconCache.hpp"
#include "io/SaveMedium.hpp"

#include <3ds.h>

#include <algorithm>
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

bool readStableCartridgeIcon(const GameDescriptor& game, std::array<std::uint16_t, 48 * 48>& pixels) {
    std::array<std::uint16_t, 48 * 48> previous{};
    for (int attempt = 0; attempt < 10; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(120'000'000LL);
        }
        IconCache::invalidate(game, true);
        if (!GameIconReader::read(game, true, pixels)) {
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
    const std::uint64_t titleId = SaveMedium::cartridgeTitleId();
    if (titleId == 0) {
        return false;
    }
    const auto mapping = std::find_if(SaveMedium::TitleMappings.begin(), SaveMedium::TitleMappings.end(),
        [&](const auto& item) { return item.titleId == titleId; });
    if (mapping == SaveMedium::TitleMappings.end()) {
        return false;
    }
    const auto match = std::find_if(games.begin(), games.end(), [&](const GameDescriptor& candidate) {
        return candidate.platform == GamePlatform::Nintendo3Ds && candidate.code == mapping->code;
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
    game.cartridge = true;
    return true;
}
}

void SaveLoadService::begin(Operation operation) {
    if (operation_ != Operation::None) {
        Logger::instance().warning("SaveLoadService::begin: rejected op="
                                   + std::to_string(static_cast<int>(operation))
                                   + ", loader busy with op="
                                   + std::to_string(static_cast<int>(operation_)));
        return;
    }
    operation_ = operation;
    jobFinished_ = false;
    progress_.store(0, std::memory_order_release);
    displayedProgress_ = 0.0F;
    switch (operation) {
        case Operation::DiscoverGames:
            discoveredGames.clear();
            phase_.store(Phase::SearchingGames, std::memory_order_release);
            break;
        case Operation::RescanCartridge:
            phase_.store(Phase::SearchingGames, std::memory_order_release);
            break;
        case Operation::CartridgeSummary:
            cartridgeSummary = {};
            break;
        case Operation::OpenGame:
            openGameResult = {};
            phase_.store(Phase::ReadingSave, std::memory_order_release);
            break;
        default:
            return;
    }
    if (!job_.start([this]() { worker(this); })) {
        phase_.store(Phase::Idle, std::memory_order_release);
        operation_ = Operation::None;
        Logger::instance().error("Save loading worker creation failed");
    }
}

SaveLoadService::Operation SaveLoadService::poll() {
    if (!jobFinished_) {
        if (!job_.poll()) {
            return Operation::None;
        }
        jobFinished_ = true;
        progress_.store(100, std::memory_order_release);
    }

    const bool showsProgressBar = operation_ == Operation::DiscoverGames
        || operation_ == Operation::OpenGame;
    if (showsProgressBar && displayedProgress_ < 99.5F) {
        return Operation::None;
    }

    const Operation completed = operation_;
    phase_.store(Phase::Idle, std::memory_order_release);
    operation_ = Operation::None;
    jobFinished_ = false;
    return completed;
}

bool SaveLoadService::blocksUi() const {
    return operation_ != Operation::None
        && operation_ != Operation::RescanCartridge
        && operation_ != Operation::CartridgeSummary;
}

void SaveLoadService::worker(void* argument) {
    auto* self = static_cast<SaveLoadService*>(argument);
    try {
        switch (self->operation_) {
            case Operation::DiscoverGames:
                self->discoverGames();
                break;
            case Operation::RescanCartridge:
                self->rescanCartridge();
                break;
            case Operation::CartridgeSummary:
                self->fetchCartridgeSummary();
                break;
            case Operation::OpenGame:
                self->openGame();
                break;
            default:
                break;
        }
    } catch (...) {
        if (self->operation_ == Operation::OpenGame) {
            self->openGameResult.success = false;
            self->openGameResult.message = "Loading failed unexpectedly.";
        }
        Logger::instance().error("Unhandled save loading worker exception");
    }
}

void SaveLoadService::discoverGames() {
    const auto games = supportedGames();
    bool cartridgeInserted = false;
    FSUSER_CardSlotIsInserted(&cartridgeInserted);

    std::size_t cartridgeCatalogIndex = games.size();
    if (cartridgeInserted) {
        waitForCardPower();
        DiscoveredGame cartridgeGame;
        if (identifyDsCartridge(games, cartridgeGame) || identify3dsCartridge(games, cartridgeGame)) {
            cartridgeCatalogIndex = cartridgeGame.catalogIndex;
            discoveredGames.push_back(std::move(cartridgeGame));
        }
    }
    progress_.store(15, std::memory_order_release);

    for (std::size_t index = 0; index < games.size(); ++index) {
        SaveAdapter candidate;
        std::string error;
        if (candidate.open(games[index], error, SaveAdapter::SourcePreference::StorageOnly)) {
            DiscoveredGame game;
            game.catalogIndex = index;
            game.save = candidate.summary();
            game.storageOnly = index == cartridgeCatalogIndex;
            discoveredGames.push_back(std::move(game));
        }
        progress_.store(15 + static_cast<int>((index + 1) * 35 / games.size()),
            std::memory_order_release);
    }

    phase_.store(Phase::ReadingIcons, std::memory_order_release);
    const std::size_t iconCount = discoveredGames.size();
    Logger::instance().info("Icon scan starting for " + std::to_string(iconCount) + " games");
    std::size_t iconIndex = 0;
    for (DiscoveredGame& game : discoveredGames) {
        game.iconPixels.reset(new (std::nothrow) std::array<std::uint16_t, 48 * 48>());
        if (game.iconPixels) {
            if (game.cartridge) {
                IconCache::invalidate(games[game.catalogIndex], true);
            }
            if (!GameIconReader::read(games[game.catalogIndex], game.cartridge, *game.iconPixels)) {
                game.iconPixels.reset();
            }
        }
        ++iconIndex;
        progress_.store(iconCount == 0 ? 100 : static_cast<int>(50 + iconIndex * 50 / iconCount),
            std::memory_order_release);
    }
    Logger::instance().info("Icon scan finished");

    digitalCache_.clear();
    for (const DiscoveredGame& game : discoveredGames) {
        if (!game.cartridge) {
            digitalCache_.push_back(game);
        }
    }
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
        progress_.store(static_cast<int>(10 + attempt * 6), std::memory_order_release);

        DiscoveredGame game;
        const bool identifiedAsDs = identifyDsCartridge(games, game);
        const bool identified = identifiedAsDs || identify3dsCartridge(games, game);
        if (!identified) {
            continue;
        }

        game.iconPixels.reset(new (std::nothrow) std::array<std::uint16_t, 48 * 48>());
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
        progress_.store(100, std::memory_order_release);
        return;
    }
    progress_.store(100, std::memory_order_release);
}

void SaveLoadService::fetchCartridgeSummary() {
    const auto games = supportedGames();
    if (catalogIndex >= games.size()) {
        return;
    }
    SaveSummary summary;
    openCartridgeSummaryStable(games[catalogIndex], summary);
    cartridgeSummary = summary;
}

void SaveLoadService::openGame() {
    const auto games = supportedGames();
    SaveAdapter& saveAdapter = app_.bankScreen_.saveAdapter();
    if (catalogIndex >= games.size()) {
        openGameResult.message = "Invalid game selection.";
        return;
    }
    if (!saveAdapter.open(games[catalogIndex], openGameResult.message, openSourcePreference)) {
        return;
    }

    openGameResult.save = saveAdapter.summary();
    openGameResult.localBox = saveAdapter.currentBox();
    openGameResult.localBoxName = saveAdapter.boxName(openGameResult.localBox);
    progress_.store(10, std::memory_order_release);
    phase_.store(Phase::SearchingPokemon, std::memory_order_release);

    openGameResult.localPokemon = saveAdapter.readBox(openGameResult.localBox);
    for (std::size_t slot = 0; slot < 30; ++slot) {
        openGameResult.localPayloads[slot] = saveAdapter.readPokemon(openGameResult.localBox, slot);
        progress_.store(10 + static_cast<int>((slot + 1) * 75 / 30), std::memory_order_release);
    }

    openGameResult.localParty = saveAdapter.readParty();
    for (std::size_t slot = 0; slot < 6; ++slot) {
        openGameResult.localPartyPayloads[slot] = saveAdapter.readPartyPokemon(slot);
        progress_.store(85 + static_cast<int>((slot + 1) * 15 / 6), std::memory_order_release);
    }

    progress_.store(100, std::memory_order_release);
    openGameResult.success = true;
}
