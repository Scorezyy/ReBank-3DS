#include "bank/LoadService.hpp"
#include "core/Logger.hpp"

#include <utility>

void LoadService::begin(Operation operation) {
    if (!claim(operation)) {
        return;
    }
    discardResult_ = false;
    accessToken_ = account_.accessToken;
    switch (operation) {
        case Operation::LoadBank:
            pendingBoxNames.clear();
            cloudBoxResult = {};
            setPhase(Phase::LoadingBank);
            break;
        case Operation::CloudBox:
            resolvedCloudBoxKey = cloudBoxKey;
            cloudBoxResult = {};
            setPhase(Phase::LoadingBank);
            break;
        case Operation::PickupCloud:
        case Operation::SwapCloud:
        case Operation::None:
            pickupResult = {};
            break;
    }
    launch([this, operation]() { work(operation); });
}

void LoadService::beginPayloadFetch(Operation operation, PayloadFetchRequest request) {
    if (busy()) {
        return;
    }
    fetch = std::move(request);
    begin(operation);
}

LoadService::Operation LoadService::poll() {
    const Operation completed = takeCompleted();
    if (completed == Operation::None || !std::exchange(discardResult_, false)) {
        return completed;
    }
    Logger::instance().info("LoadService: dropped a result that belonged to the previous account");
    return Operation::None;
}

void LoadService::work(Operation operation) {
    const bool listing = operation == Operation::LoadBank || operation == Operation::CloudBox;
    try {
        if (operation == Operation::LoadBank) {
            cloudBoxResult = api_.listCloudBox(1, accessToken_);
            pendingBoxNames = api_.listBoxNames(accessToken_).boxes;
        } else if (operation == Operation::CloudBox) {
            cloudBoxResult = api_.listCloudBox(static_cast<std::uint16_t>(resolvedCloudBoxKey + 1), accessToken_);
        } else {
            pickupResult = api_.downloadPokemon({fetch.cloudBox, static_cast<std::uint8_t>(fetch.slot + 1)},
                                                accessToken_);
        }
        if (listing) {
            setProgress(100);
        }
    } catch (...) {
        if (listing) {
            cloudBoxResult.success = false;
            cloudBoxResult.message = "Bank loading failed unexpectedly.";
        } else {
            pickupResult.success = false;
            pickupResult.message = "Fetch failed unexpectedly.";
        }
        Logger::instance().error("Unhandled loading worker exception");
    }
}
