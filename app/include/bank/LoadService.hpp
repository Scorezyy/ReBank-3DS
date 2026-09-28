#pragma once

#include "core/BackgroundOperation.hpp"
#include "network/ApiClient.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PayloadFetchRequest {
    std::size_t slot = 0;
    std::uint16_t cloudBox = 0;
    PokemonSummary summary;
    std::uint32_t handGeneration = 0;
};

enum class LoadOperation {
    None,
    LoadBank,
    CloudBox,
    PickupCloud,
    SwapCloud
};

enum class LoadPhase {
    Idle,
    LoadingBank
};

class LoadService : public BackgroundOperation<LoadOperation, LoadPhase> {
public:
    using Operation = LoadOperation;
    using Phase = LoadPhase;

    LoadService(ApiClient& api, const AccountSession& account)
        : BackgroundOperation("LoadService"), api_(api), account_(account) {}

    void begin(Operation operation);
    void beginPayloadFetch(Operation operation, PayloadFetchRequest request);
    Operation poll();
    void invalidate() { discardResult_ = busy(); }
    bool blocksUi() const { return operation() == Operation::LoadBank; }

    std::uint16_t cloudBoxKey = 0;
    std::uint16_t resolvedCloudBoxKey = 0;
    PayloadFetchRequest fetch;

    BoxListResult cloudBoxResult;
    DownloadResult pickupResult;
    std::vector<BoxNameEntry> pendingBoxNames;

private:
    void work(Operation operation);

    ApiClient& api_;
    const AccountSession& account_;
    bool discardResult_ = false;
    std::string accessToken_;
};
