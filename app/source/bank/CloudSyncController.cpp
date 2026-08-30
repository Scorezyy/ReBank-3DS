#include "bank/CloudSyncController.hpp"

#include "app/App.hpp"
#include "core/Logger.hpp"

bool CloudSyncController::blockedByOtherWork() const {
    return app_.loadService_.running() || renameController_.isRunning() || commit_.running();
}

void CloudSyncController::pumpHandPayloadFetch() {
    if (!session_.hand.active || session_.hand.source != HandSource::Cloud || session_.hand.payloadKnown) {
        return;
    }
    if (app_.loadService_.running()) {
        return;
    }
    app_.loadService_.pickupSlot = session_.hand.sourceIndex;
    app_.loadService_.pickupCloudBox = session_.hand.sourceCloudBox;
    app_.loadService_.pickupSummary = session_.hand.summary;
    app_.loadService_.pickupHandGeneration = session_.handGeneration;
    app_.loadService_.begin(LoadService::Operation::PickupCloud);
}

void CloudSyncController::pumpCloudPayloadPrefetch() {
    if (session_.hand.active || session_.storagePane != StoragePane::Cloud || commit_.requested()
        || session_.trashBoxActive) {
        return;
    }
    if (blockedByOtherWork()) {
        return;
    }
    for (std::size_t slot = 0; slot < session_.cloudPreview.size(); ++slot) {
        if (session_.cloudPreview[slot].species == 0) {
            continue;
        }
        if (!session_.cachedCloudPayloads[slot].data.empty() || session_.payloadPrefetchFailed[slot]) {
            continue;
        }
        if (!session_.pendingUploadPayloads[slot].data.empty()) {
            continue;
        }
        Logger::instance().info("pumpCloudPayloadPrefetch: starting fetch for box "
                                + std::to_string(session_.cloudBox + 1) + " slot " + std::to_string(slot + 1));
        app_.loadService_.pickupSlot = slot;
        app_.loadService_.pickupCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
        app_.loadService_.pickupSummary = session_.cloudPreview[slot];
        app_.loadService_.begin(LoadService::Operation::PickupCloud);
        return;
    }
}

bool CloudSyncController::nextCloudPrefetchKey(std::uint16_t& outKey) const {
    if (app_.session_.accessToken.empty()) {
        return false;
    }
    const std::size_t boxLimit = app_.session_.boxLimit == 0 ? 50 : app_.session_.boxLimit;
    const u64 now = svcGetSystemTick();

    const auto onCooldown = [&](std::uint16_t key) {
        const auto it = session_.cloudPrefetchCooldownUntil.find(key);
        return it != session_.cloudPrefetchCooldownUntil.end() && now < it->second;
    };
    const auto centerKey = static_cast<std::uint16_t>(session_.cloudBox);
    if (!session_.cloudBoxes.count(centerKey) && !onCooldown(centerKey)) {
        outKey = centerKey;
        return true;
    }
    constexpr std::size_t maxPrefetchDistance = 3;
    for (std::size_t distance = 1; distance <= maxPrefetchDistance && distance < boxLimit; ++distance) {
        const auto leftKey = static_cast<std::uint16_t>((session_.cloudBox + boxLimit - distance) % boxLimit);
        if (!session_.cloudBoxes.count(leftKey) && !onCooldown(leftKey)) {
            outKey = leftKey;
            return true;
        }
        const auto rightKey = static_cast<std::uint16_t>((session_.cloudBox + distance) % boxLimit);
        if (rightKey != leftKey && !session_.cloudBoxes.count(rightKey) && !onCooldown(rightKey)) {
            outKey = rightKey;
            return true;
        }
    }
    return false;
}

void CloudSyncController::pumpCloudPrefetch() {
    if (commit_.requested() || session_.storagePane != StoragePane::Cloud || session_.trashBoxActive) {
        return;
    }
    if (blockedByOtherWork()) {
        return;
    }
    std::uint16_t key = 0;
    if (!nextCloudPrefetchKey(key)) {
        return;
    }
    Logger::instance().info("pumpCloudPrefetch: starting fetch for box " + std::to_string(key + 1));
    app_.loadService_.cloudBoxKey = key;
    app_.loadService_.begin(LoadService::Operation::CloudBox);
}

void CloudSyncController::onCloudBoxLoaded() {
    const auto boxKey = app_.loadService_.resolvedCloudBoxKey;
    const BoxListResult& result = app_.loadService_.cloudBoxResult;
    Logger::instance().info("onCloudBoxLoaded: box " + std::to_string(boxKey + 1)
                            + " success=" + std::to_string(result.success)
                            + " currentCloudBox=" + std::to_string(session_.cloudBox + 1));
    if (result.success) {
        CloudBoxDraft draft;
        draft.baseline = result.pokemon;
        draft.summaries = result.pokemon;
        draft.payloads = result.payloads;
        session_.cloudBoxes[boxKey] = std::move(draft);
        session_.cloudPrefetchCooldownUntil.erase(boxKey);
        if (session_.cloudBox == boxKey) {
            session_.cloudPreview = session_.cloudBoxes[boxKey].summaries;
            session_.pendingUploadPayloads = session_.cloudBoxes[boxKey].pending;
            session_.cachedCloudPayloads = session_.cloudBoxes[boxKey].payloads;
            session_.payloadPrefetchFailed = {};
        }
        app_.status_.clear();
        return;
    }
    if (session_.cloudBox == boxKey) {
        session_.cloudPreview.fill({});
        session_.pendingUploadPayloads = {};
    }

    session_.cloudPrefetchCooldownUntil[boxKey] = svcGetSystemTick() + static_cast<u64>(15.0 * SYSCLOCK_ARM11);
    app_.status_ = result.message;
    Logger::instance().warning("Cloud box refresh failed: " + app_.status_);
}

void CloudSyncController::onCloudPickupCompleted() {
    const bool stillHeld = session_.hand.active
        && session_.hand.source == HandSource::Cloud
        && session_.hand.sourceIndex == app_.loadService_.pickupSlot
        && !session_.hand.payloadKnown
        && session_.handGeneration == app_.loadService_.pickupHandGeneration;
    const bool viewingPickupBox =
        app_.loadService_.pickupCloudBox == static_cast<std::uint16_t>(session_.cloudBox + 1);
    DownloadResult& result = app_.loadService_.pickupResult;
    if (result.success) {
        PokemonPayload payload;
        payload.format = result.pokemon.format;
        payload.data = std::move(result.pokemon.payload);
        if (viewingPickupBox) {
            session_.cachedCloudPayloads[app_.loadService_.pickupSlot] = payload;
        }
        const auto pickupBoxKey = static_cast<std::uint16_t>(app_.loadService_.pickupCloudBox - 1);
        auto pickupBoxIt = session_.cloudBoxes.find(pickupBoxKey);
        if (pickupBoxIt != session_.cloudBoxes.end() && app_.loadService_.pickupSlot < 30) {
            pickupBoxIt->second.payloads[app_.loadService_.pickupSlot] = payload;
        }
        if (stillHeld) {
            session_.hand.payload = std::move(payload);
            session_.hand.payloadKnown = !session_.hand.payload.data.empty();
        }
        return;
    }
    if (!stillHeld) {
        if (viewingPickupBox && app_.loadService_.pickupSlot < session_.payloadPrefetchFailed.size()) {
            session_.payloadPrefetchFailed[app_.loadService_.pickupSlot] = true;
        }
        Logger::instance().warning("Cloud payload prefetch failed: " + result.message);
        return;
    }
    if (viewingPickupBox) {
        session_.cloudPreview[app_.loadService_.pickupSlot] = session_.hand.summary;
    }
    session_.hand = Hand{};
    app_.status_ = "Cannot pick up: " + result.message;
    session_.errorDialogTitle = "PICKUP FAILED";
    session_.errorDialogPokemon = app_.loadService_.pickupSummary.nickname.empty()
        ? "Unknown Pokemon"
        : app_.loadService_.pickupSummary.nickname;
    session_.errorDialogLocation.clear();
    session_.errorDialogMessage = result.message.empty()
        ? "The Pokemon payload could not be read."
        : result.message;
    session_.errorDialogVisible = true;
    Logger::instance().warning("Cloud pickup failed: " + result.message);
}

void CloudSyncController::onCloudSwapCompleted() {
    DownloadResult& result = app_.loadService_.pickupResult;
    if (!result.success) {
        app_.status_ = "Cannot swap: " + result.message;
        session_.errorDialogTitle = "SWAP FAILED";
        session_.errorDialogPokemon = app_.loadService_.pickupSummary.nickname.empty()
            ? "Unknown Pokemon"
            : app_.loadService_.pickupSummary.nickname;
        session_.errorDialogLocation.clear();
        session_.errorDialogMessage = result.message.empty()
            ? "The Pokemon payload could not be read."
            : result.message;
        session_.errorDialogVisible = true;
        Logger::instance().warning("Cloud swap failed: " + result.message);
        return;
    }
    const bool stillValid = session_.hand.active
        && session_.handGeneration == app_.loadService_.pickupHandGeneration;
    const bool viewingSwapBox =
        app_.loadService_.pickupCloudBox == static_cast<std::uint16_t>(session_.cloudBox + 1);
    if (!stillValid || !viewingSwapBox) {
        const auto swapBoxKey = static_cast<std::uint16_t>(app_.loadService_.pickupCloudBox - 1);
        auto swapBoxIt = session_.cloudBoxes.find(swapBoxKey);
        if (swapBoxIt != session_.cloudBoxes.end() && app_.loadService_.pickupSlot < 30) {
            swapBoxIt->second.payloads[app_.loadService_.pickupSlot].format = result.pokemon.format;
            swapBoxIt->second.payloads[app_.loadService_.pickupSlot].data = result.pokemon.payload;
        }
        if (viewingSwapBox) {
            session_.cachedCloudPayloads[app_.loadService_.pickupSlot].format = result.pokemon.format;
            session_.cachedCloudPayloads[app_.loadService_.pickupSlot].data = std::move(result.pokemon.payload);
        }
        app_.status_ = stillValid ? "Box changed, swap cancelled." : "Swap cancelled.";
        return;
    }
    PokemonPayload occupantPayload;
    occupantPayload.format = result.pokemon.format;
    occupantPayload.data = std::move(result.pokemon.payload);
    const auto swapBoxKey = static_cast<std::uint16_t>(app_.loadService_.pickupCloudBox - 1);
    auto swapBoxIt = session_.cloudBoxes.find(swapBoxKey);
    if (swapBoxIt != session_.cloudBoxes.end() && app_.loadService_.pickupSlot < 30) {
        swapBoxIt->second.payloads[app_.loadService_.pickupSlot] = occupantPayload;
    }
    const PokemonSummary occupantSummary = session_.cloudPreview[app_.loadService_.pickupSlot];
    session_.cloudPreview[app_.loadService_.pickupSlot] = session_.hand.summary;
    session_.pendingUploadPayloads[app_.loadService_.pickupSlot] = session_.hand.payload;
    session_.hand.summary = occupantSummary;
    session_.hand.payload = std::move(occupantPayload);
    session_.hand.source = HandSource::Cloud;
    session_.hand.sourceIndex = app_.loadService_.pickupSlot;
    session_.hand.sourceCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
    session_.hand.payloadKnown = !session_.hand.payload.data.empty();
    ++session_.handGeneration;
    app_.status_ = session_.hand.summary.nickname + " swapped.";
}

void CloudSyncController::beginRenameBox(std::uint16_t position, std::string name) {
    if (renameController_.isRunning()) {
        return;
    }
    if (app_.session_.accessToken.empty()) {
        app_.status_ = "Please sign in again.";
        return;
    }
    renameController_.begin(app_.api_, position, std::move(name), app_.session_.accessToken);
}

void CloudSyncController::pollRenameBox() {
    std::uint16_t position = 0;
    RenameBoxResult result;
    if (!renameController_.poll(position, result)) {
        return;
    }
    if (result.success) {
        session_.cloudBoxNames[position] = result.name;
    }
}
