#pragma once

#include "core/AsyncJob.hpp"
#include "network/ApiClient.hpp"
#include "save/SaveAdapter.hpp"

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
    std::unique_ptr<std::array<std::uint16_t, 48 * 48>> iconPixels;
};

class LoadService {
public:
    enum class Operation {
        None,
        DiscoverGames,
        OpenGame,
        LoadBank,
        CloudBox,
        PickupCloud,
        SwapCloud
    };

    enum class Phase {
        Idle,
        SearchingGames,
        ReadingIcons,
        ReadingSave,
        SearchingPokemon,
        LoadingBank
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

    explicit LoadService(App& app) : app_(app) {}

    void begin(Operation operation);
    Operation poll();

    bool running() const { return job_.running(); }
    Operation operation() const { return operation_; }

    bool blocksUi() const;

    Phase phase() const { return phase_.load(std::memory_order_acquire); }
    int progress() const { return progress_.load(std::memory_order_acquire); }
    float& displayedProgress() { return displayedProgress_; }

    std::size_t catalogIndex = 0;
    std::uint16_t cloudBoxKey = 0;
    std::uint16_t resolvedCloudBoxKey = 0;
    std::size_t pickupSlot = 0;
    std::uint16_t pickupCloudBox = 0;
    PokemonSummary pickupSummary;
    // BankSession::handGeneration captured when a PickupCloud/SwapCloud fetch
    // starts. If the hand has moved on (returned, or something else picked
    // up) by the time the fetch resolves, the generation no longer matches
    // and the stale result must not be applied to whatever the hand holds
    // now - otherwise it can silently overwrite an unrelated cloud slot.
    std::uint32_t pickupHandGeneration = 0;

    std::vector<DiscoveredGame> discoveredGames;
    OpenGameResult openGameResult;
    BoxListResult cloudBoxResult;
    DownloadResult pickupResult;
    std::vector<BoxNameEntry> pendingBoxNames;

private:
    static void worker(void* argument);
    bool jobFinished_ = false;

    App& app_;
    AsyncJob job_;
    Operation operation_ = Operation::None;
    std::atomic<Phase> phase_{Phase::Idle};
    std::atomic<int> progress_{0};
    float displayedProgress_ = 0.0F;
};
