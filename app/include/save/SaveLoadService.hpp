#pragma once

#include "core/AsyncJob.hpp"
#include "save/adapter/SaveAdapter.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class App;
struct DiscoveredGame {
    std::size_t catalogIndex = 0;
    SaveSummary save;
    bool cartridge = false;
    bool storageOnly = false;
    std::shared_ptr<std::array<std::uint16_t, 48 * 48>> iconPixels;
};

class SaveLoadService {
public:
    enum class Operation {
        None,
        DiscoverGames,
        RescanCartridge,
        CartridgeSummary,
        OpenGame
    };

    enum class Phase {
        Idle,
        SearchingGames,
        ReadingIcons,
        ReadingSave,
        SearchingPokemon
    };

    struct OpenGameResult {
        bool success = false;
        std::string message;
        SaveSummary save;
        std::size_t localBox = 0;
        std::string localBoxName;
        std::array<PokemonSummary, 30> localPokemon{};
        std::array<PokemonPayload, 30> localPayloads{};
        std::array<PokemonSummary, 6> localParty{};
        std::array<PokemonPayload, 6> localPartyPayloads{};
    };

    explicit SaveLoadService(App& app) : app_(app) {}

    void begin(Operation operation);
    Operation poll();
    void dropCartridgeGames() { discoveredGames = digitalCache_; }

    bool running() const { return job_.running(); }
    Operation operation() const { return operation_; }

    bool blocksUi() const;

    Phase phase() const { return phase_.load(std::memory_order_acquire); }
    int progress() const { return progress_.load(std::memory_order_acquire); }
    float& displayedProgress() { return displayedProgress_; }

    std::size_t catalogIndex = 0;
    SaveAdapter::SourcePreference openSourcePreference = SaveAdapter::SourcePreference::Any;

    std::vector<DiscoveredGame> discoveredGames;
    OpenGameResult openGameResult;
    SaveSummary cartridgeSummary;

private:
    static void worker(void* argument);
    void discoverGames();
    void rescanCartridge();
    void fetchCartridgeSummary();
    void openGame();
    bool jobFinished_ = false;

    App& app_;
    AsyncJob job_;
    std::vector<DiscoveredGame> digitalCache_;
    Operation operation_ = Operation::None;
    std::atomic<Phase> phase_{Phase::Idle};
    std::atomic<int> progress_{0};
    float displayedProgress_ = 0.0F;
};
