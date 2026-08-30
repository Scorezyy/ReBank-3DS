#include "network/LoadService.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"

void LoadService::begin(Operation operation) {
    if (operation_ != Operation::None) {
        Logger::instance().warning("LoadService::begin: rejected op="
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
        case Operation::LoadBank:
            cloudBoxResult = {};
            pendingBoxNames.clear();
            phase_.store(Phase::LoadingBank, std::memory_order_release);
            break;
        case Operation::CloudBox:
            cloudBoxResult = {};
            resolvedCloudBoxKey = cloudBoxKey;
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

    const bool showsProgressBar = operation_ == Operation::LoadBank;
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
        if (self->operation_ == Operation::LoadBank) {
            self->cloudBoxResult = app.api_.listCloudBox(1, app.session_.accessToken);
            self->pendingBoxNames = app.api_.listBoxNames(app.session_.accessToken).boxes;
            self->progress_.store(100, std::memory_order_release);
        } else if (self->operation_ == Operation::CloudBox) {
            self->cloudBoxResult = app.api_.listCloudBox(
                static_cast<std::uint16_t>(self->resolvedCloudBoxKey + 1),
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
        if (self->operation_ == Operation::LoadBank
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
