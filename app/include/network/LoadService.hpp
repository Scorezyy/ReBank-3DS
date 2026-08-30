#pragma once

#include "core/AsyncJob.hpp"
#include "network/ApiClient.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class App;

class LoadService {
public:
    enum class Operation {
        None,
        LoadBank,
        CloudBox,
        PickupCloud,
        SwapCloud
    };

    enum class Phase {
        Idle,
        LoadingBank
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

    std::uint16_t cloudBoxKey = 0;
    std::uint16_t resolvedCloudBoxKey = 0;
    std::size_t pickupSlot = 0;
    std::uint16_t pickupCloudBox = 0;
    PokemonSummary pickupSummary;
    
    std::uint32_t pickupHandGeneration = 0;

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
