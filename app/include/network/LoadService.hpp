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

// Save data staged while the background worker scans for compatible games;
// GameSelectScreen turns each entry into a display profile (with a built
// icon texture) once discovery finishes.
struct DiscoveredGame {
    std::size_t catalogIndex = 0;
    SaveSummary save;
    bool cartridge = false;
    std::unique_ptr<std::array<std::uint16_t, 48 * 48>> iconPixels;
};

// Runs every long-running read on a single shared background thread: game
// discovery, opening the local save, and fetching a cloud box or a single
// cloud Pokemon. One job at a time, matching how the previous single-thread
// App::loadWorker behaved - callers stage the operation's input fields
// before calling begin(), then read the matching result field once poll()
// reports that operation finished.
class LoadService {
public:
    enum class Operation {
        None,
        DiscoverGames,
        OpenGame,
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
        BoxListResult cloudBox;
    };

    explicit LoadService(App& app) : app_(app) {}

    void begin(Operation operation);
    // Returns the operation that just finished, or Operation::None if
    // nothing completed this frame.
    Operation poll();

    bool running() const { return job_.running(); }
    Operation operation() const { return operation_; }
    // True while a load should block all input and other screens - excludes
    // the box-focused fetches that run while the player keeps browsing
    // (CloudBox/PickupCloud/SwapCloud).
    bool blocksUi() const;

    Phase phase() const { return phase_.load(std::memory_order_acquire); }
    int progress() const { return progress_.load(std::memory_order_acquire); }
    float& displayedProgress() { return displayedProgress_; }

    // Operation inputs, set by the caller before begin().
    std::size_t catalogIndex = 0;
    std::uint16_t cloudBoxKey = 0;
    std::size_t pickupSlot = 0;
    std::uint16_t pickupCloudBox = 0;
    PokemonSummary pickupSummary;

    // Operation results, valid once poll() reports the matching operation.
    std::vector<DiscoveredGame> discoveredGames;
    OpenGameResult openGameResult;
    BoxListResult cloudBoxResult;
    DownloadResult pickupResult;
    std::vector<BoxNameEntry> pendingBoxNames;

private:
    static void worker(void* argument);

    App& app_;
    AsyncJob job_;
    Operation operation_ = Operation::None;
    std::atomic<Phase> phase_{Phase::Idle};
    std::atomic<int> progress_{0};
    float displayedProgress_ = 0.0F;
};
