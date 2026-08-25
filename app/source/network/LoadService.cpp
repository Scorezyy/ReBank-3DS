#include "network/LoadService.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "save/GameCatalog.hpp"
#include "save/GameIconReader.hpp"

#include <array>
#include <new>

void LoadService::begin(Operation operation) {
    if (operation_ != Operation::None) {
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
        case Operation::OpenGame:
            openGameResult = {};
            phase_.store(Phase::ReadingSave, std::memory_order_release);
            break;
        case Operation::LoadBank:
            cloudBoxResult = {};
            pendingBoxNames.clear();
            phase_.store(Phase::LoadingBank, std::memory_order_release);
            break;
        case Operation::CloudBox:
            cloudBoxResult = {};
            phase_.store(Phase::LoadingBank, std::memory_order_release);
            break;
        case Operation::PickupCloud:
        case Operation::SwapCloud:
            pickupResult = {};
            break;
        default:
            return;
    }
    if (!job_.start([this]() { worker(this); })) {
        phase_.store(Phase::Idle, std::memory_order_release);
        operation_ = Operation::None;
        Logger::instance().error("Loading worker creation failed");
    }
}

LoadService::Operation LoadService::poll() {
    if (!jobFinished_) {
        if (!job_.poll()) {
            return Operation::None;
        }
        jobFinished_ = true;
        progress_.store(100, std::memory_order_release);
    }

    const bool showsProgressBar = operation_ == Operation::DiscoverGames
        || operation_ == Operation::OpenGame
        || operation_ == Operation::LoadBank;
    if (showsProgressBar && displayedProgress_ < 99.5F) {
        return Operation::None;
    }

    const Operation completed = operation_;
    phase_.store(Phase::Idle, std::memory_order_release);
    operation_ = Operation::None;
    jobFinished_ = false;
    return completed;
}

bool LoadService::blocksUi() const {
    return operation_ != Operation::None
        && operation_ != Operation::CloudBox
        && operation_ != Operation::PickupCloud
        && operation_ != Operation::SwapCloud;
}

void LoadService::worker(void* argument) {
    auto* self = static_cast<LoadService*>(argument);
    App& app = self->app_;
    try {
        if (self->operation_ == Operation::DiscoverGames) {
            const auto games = supportedGames();
            for (std::size_t index = 0; index < games.size(); ++index) {
                SaveAdapter candidate;
                std::string error;
                if (!candidate.open(games[index], error)) {
                    continue;
                }
                DiscoveredGame game;
                game.catalogIndex = index;
                game.save = candidate.summary();
                game.cartridge = candidate.isCartridge();
                self->discoveredGames.push_back(std::move(game));
                self->progress_.store(
                    games.empty() ? 0 : static_cast<int>((index + 1) * 50 / games.size()),
                    std::memory_order_release);
            }
            self->phase_.store(Phase::ReadingIcons, std::memory_order_release);
            const std::size_t iconCount = self->discoveredGames.size();
            Logger::instance().info(
                "Icon scan starting for " + std::to_string(iconCount) + " games");
            std::size_t iconIndex = 0;
            for (DiscoveredGame& game : self->discoveredGames) {
                game.iconPixels.reset(
                    new (std::nothrow) std::array<std::uint16_t, 48 * 48>());
                if (game.iconPixels
                    && !GameIconReader::read(
                        games[game.catalogIndex], game.cartridge, *game.iconPixels)) {
                    game.iconPixels.reset();
                }
                ++iconIndex;
                self->progress_.store(
                    iconCount == 0 ? 100 : static_cast<int>(50 + iconIndex * 50 / iconCount),
                    std::memory_order_release);
            }
            Logger::instance().info("Icon scan finished");
        } else if (self->operation_ == Operation::OpenGame) {
            const auto games = supportedGames();
            SaveAdapter& saveAdapter = app.bankScreen_.saveAdapter();
            if (self->catalogIndex >= games.size()) {
                self->openGameResult.message = "Invalid game selection.";
            } else if (saveAdapter.open(
                           games[self->catalogIndex], self->openGameResult.message)) {
                self->openGameResult.save = saveAdapter.summary();
                self->openGameResult.localBox = saveAdapter.currentBox();
                self->openGameResult.localBoxName =
                    saveAdapter.boxName(self->openGameResult.localBox);
                self->progress_.store(10, std::memory_order_release);
                self->phase_.store(Phase::SearchingPokemon, std::memory_order_release);
                self->openGameResult.localPokemon =
                    saveAdapter.readBox(self->openGameResult.localBox);
                for (std::size_t slot = 0; slot < 30; ++slot) {
                    self->openGameResult.localPayloads[slot] =
                        saveAdapter.readPokemon(self->openGameResult.localBox, slot);
                    self->progress_.store(
                        10 + static_cast<int>((slot + 1) * 80 / 30),
                        std::memory_order_release);
                }
                self->progress_.store(100, std::memory_order_release);
                self->openGameResult.success = true;
            }
        } else if (self->operation_ == Operation::LoadBank) {
            self->cloudBoxResult = app.api_.listCloudBox(1, app.session_.accessToken);
            self->pendingBoxNames = app.api_.listBoxNames(app.session_.accessToken).boxes;
            self->progress_.store(100, std::memory_order_release);
        } else if (self->operation_ == Operation::CloudBox) {
            self->cloudBoxResult = app.api_.listCloudBox(
                static_cast<std::uint16_t>(self->cloudBoxKey + 1),
                app.session_.accessToken);
            self->progress_.store(100, std::memory_order_release);
        } else if (self->operation_ == Operation::PickupCloud
                   || self->operation_ == Operation::SwapCloud) {
            self->pickupResult = app.api_.downloadPokemon(
                self->pickupCloudBox,
                static_cast<std::uint8_t>(self->pickupSlot + 1),
                app.session_.accessToken);
        }
    } catch (...) {
        if (self->operation_ == Operation::OpenGame) {
            self->openGameResult.success = false;
            self->openGameResult.message = "Loading failed unexpectedly.";
        } else if (self->operation_ == Operation::LoadBank
                   || self->operation_ == Operation::CloudBox) {
            self->cloudBoxResult.success = false;
            self->cloudBoxResult.message = "Bank loading failed unexpectedly.";
        } else if (self->operation_ == Operation::PickupCloud
                   || self->operation_ == Operation::SwapCloud) {
            self->pickupResult.success = false;
            self->pickupResult.message = "Fetch failed unexpectedly.";
        }
        Logger::instance().error("Unhandled loading worker exception");
    }
}
