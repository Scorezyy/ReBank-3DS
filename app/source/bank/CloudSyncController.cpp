#include "bank/CloudSyncController.hpp"

#include "core/Logger.hpp"

#include <utility>

namespace {
u64 ticksFromNow(double seconds) {
    return svcGetSystemTick() + static_cast<u64>(seconds * SYSCLOCK_ARM11);
}
}

void CloudSyncController::pumpBackgroundFetches() {
    pumpHandPayloadFetch();
    pumpHeldRegionPayloadFetch();
    pumpCloudPayloadPrefetch();
    pumpCloudPrefetch();
}

bool CloudSyncController::blockedByOtherWork() const {
    return context_.loads.busy() || rename_.running() || commit_.running();
}

void CloudSyncController::fetchPayload(std::size_t slot, std::uint16_t cloudPosition, const PokemonSummary& summary) {
    context_.loads.beginPayloadFetch(LoadService::Operation::PickupCloud,
        PayloadFetchRequest{slot, cloudPosition, summary, session_.handGeneration});
}

void CloudSyncController::rememberCloudPayload(std::uint16_t cloudPosition, std::size_t slot,
                                               const PokemonPayload& payload) {
    if (cloudPosition == 0 || slot >= BoxSlotCount) {
        return;
    }
    const auto boxKey = static_cast<std::uint16_t>(cloudPosition - 1);
    if (const auto box = session_.cloudBoxes.find(boxKey); box != session_.cloudBoxes.end()) {
        box->second.payloads[slot] = payload;
    }
    if (boxKey == session_.cloudKey()) {
        session_.cloud.cached[slot] = payload;
    }
}

void CloudSyncController::showFetchError(TextId title, const std::string& message) {
    const PokemonSummary& summary = context_.loads.fetch.summary;
    context_.errors.show(std::string(context_.text.get(title)),
        message.empty() ? std::string(context_.text.get(TextId::PayloadUnreadable)) : message,
        summary.nickname.empty() ? std::string(context_.text.get(TextId::UnknownPokemon)) : summary.nickname);
}

void CloudSyncController::pumpHandPayloadFetch() {
    const Hand& hand = session_.hand;
    if (!hand.active || !hand.source.isCloudBank() || hand.payloadKnown || context_.loads.busy()) {
        return;
    }
    fetchPayload(hand.sourceIndex, hand.sourceCloudBox, hand.summary);
}

void CloudSyncController::pumpCloudPayloadPrefetch() {
    if (session_.hand.active || selection_.engaged() || session_.storagePane != StoragePane::Cloud
        || commit_.requested() || session_.trashBoxActive || blockedByOtherWork()
        || svcGetSystemTick() < regionFetchRetryAt_) {
        return;
    }
    const CloudView& cloud = session_.cloud;
    for (std::size_t slot = 0; slot < BoxSlotCount; ++slot) {
        if (!cloud.summaries[slot].occupied() || cloud.cached[slot].known() || cloud.prefetchFailed[slot]
            || cloud.pending[slot].known()) {
            continue;
        }
        Logger::instance().info("pumpCloudPayloadPrefetch: starting fetch for box "
                                + std::to_string(session_.cloudPosition()) + " slot " + std::to_string(slot + 1));
        fetchPayload(slot, session_.cloudPosition(), cloud.summaries[slot]);
        return;
    }
}

void CloudSyncController::pumpHeldRegionPayloadFetch() {
    if (blockedByOtherWork() || svcGetSystemTick() < regionFetchRetryAt_) {
        return;
    }
    const std::optional<RegionPayloadRequest> request = selection_.nextPayloadRequest();
    if (!request) {
        return;
    }
    Logger::instance().info("region payload fetch: bank " + std::to_string(request->cloudBox) + " slot "
                            + std::to_string(request->slot + 1));
    fetchPayload(request->slot, request->cloudBox, request->summary);
}

std::optional<std::uint16_t> CloudSyncController::nextCloudPrefetchKey() const {
    if (!context_.signedIn()) {
        return std::nullopt;
    }
    const std::size_t boxLimit = context_.cloudBoxLimit();
    const u64 now = svcGetSystemTick();
    const auto available = [&](std::uint16_t key) {
        if (session_.cloudBoxes.count(key)) {
            return false;
        }
        const auto cooldown = session_.cloudPrefetchCooldownUntil.find(key);
        return cooldown == session_.cloudPrefetchCooldownUntil.end() || now >= cooldown->second;
    };
    if (available(session_.cloudKey())) {
        return session_.cloudKey();
    }
    constexpr std::size_t MaxPrefetchDistance = 3;
    for (std::size_t distance = 1; distance <= MaxPrefetchDistance && distance < boxLimit; ++distance) {
        const auto leftKey = static_cast<std::uint16_t>((session_.cloudBox + boxLimit - distance) % boxLimit);
        if (available(leftKey)) {
            return leftKey;
        }
        const auto rightKey = static_cast<std::uint16_t>((session_.cloudBox + distance) % boxLimit);
        if (rightKey != leftKey && available(rightKey)) {
            return rightKey;
        }
    }
    return std::nullopt;
}

void CloudSyncController::pumpCloudPrefetch() {
    if (commit_.requested() || session_.storagePane != StoragePane::Cloud || session_.trashBoxActive
        || (selection_.holding() && !session_.cloud.awaitingLoad) || blockedByOtherWork()) {
        return;
    }
    const std::optional<std::uint16_t> key = nextCloudPrefetchKey();
    if (!key) {
        return;
    }
    Logger::instance().info("pumpCloudPrefetch: starting fetch for box " + std::to_string(*key + 1));
    context_.loads.cloudBoxKey = *key;
    context_.loads.begin(LoadService::Operation::CloudBox);
}

void CloudSyncController::onCloudBoxLoaded() {
    const auto boxKey = context_.loads.resolvedCloudBoxKey;
    const BoxListResult& result = context_.loads.cloudBoxResult;
    const bool current = session_.cloudKey() == boxKey;
    Logger::instance().info("onCloudBoxLoaded: box " + std::to_string(boxKey + 1)
                            + " success=" + std::to_string(result.success)
                            + " currentCloudBox=" + std::to_string(session_.cloudPosition()));
    if (!result.success) {
        if (current) {
            session_.cloud.clear(false);
        }
        session_.cloudPrefetchCooldownUntil[boxKey] = ticksFromNow(15.0);
        context_.status = result.message;
        Logger::instance().warning("Cloud box refresh failed: " + context_.status);
        return;
    }
    auto existing = session_.cloudBoxes.find(boxKey);
    if (existing == session_.cloudBoxes.end()) {
        existing = session_.cloudBoxes.emplace(boxKey, CloudBoxDraft::fromServer(result.pokemon, result.payloads)).first;
    } else {
        existing->second.baseline = result.pokemon;
        existing->second.payloads = result.payloads;
        Logger::instance().info("onCloudBoxLoaded: kept local edits for box " + std::to_string(boxKey + 1));
    }
    session_.cloudPrefetchCooldownUntil.erase(boxKey);
    if (current) {
        session_.cloud.show(existing->second);
    }
    context_.status.clear();
}

void CloudSyncController::onCloudPickupCompleted() {
    const PayloadFetchRequest& fetch = context_.loads.fetch;
    Hand& hand = session_.hand;
    const bool stillHeld = hand.active
        && hand.source.isCloudBank()
        && hand.sourceIndex == fetch.slot
        && hand.sourceCloudBox == fetch.cloudBox
        && !hand.payloadKnown
        && session_.handGeneration == fetch.handGeneration;
    DownloadResult& result = context_.loads.pickupResult;
    if (result.success) {
        rememberCloudPayload(fetch.cloudBox, fetch.slot, result.payload);
        selection_.deliverPayload(fetch.cloudBox, fetch.slot, result.payload);
        selection_.retryPlacement();
        if (stillHeld) {
            hand.payload = std::move(result.payload);
            hand.payloadKnown = hand.payload.known();
        }
        return;
    }
    const bool viewingFetchedBox = fetch.cloudBox == session_.cloudPosition();
    if (!stillHeld) {
        if (selection_.holding()) {
            selection_.failPayload(fetch.cloudBox, fetch.slot);
            regionFetchRetryAt_ = ticksFromNow(3.0);
        } else if (viewingFetchedBox && fetch.slot < BoxSlotCount) {
            session_.cloud.prefetchFailed[fetch.slot] = true;
        }
        Logger::instance().warning("Cloud payload prefetch failed: " + result.message);
        return;
    }
    if (viewingFetchedBox) {
        session_.cloud.summaries[fetch.slot] = hand.summary;
    }
    hand = Hand{};
    context_.status = context_.text.format(TextId::CannotPickUp, {result.message});
    showFetchError(TextId::PickupFailedTitle, result.message);
    Logger::instance().warning("Cloud pickup failed: " + result.message);
}

void CloudSyncController::onCloudSwapCompleted() {
    const PayloadFetchRequest& fetch = context_.loads.fetch;
    DownloadResult& result = context_.loads.pickupResult;
    if (!result.success) {
        context_.status = context_.text.format(TextId::CannotSwap, {result.message});
        showFetchError(TextId::SwapFailedTitle, result.message);
        Logger::instance().warning("Cloud swap failed: " + result.message);
        return;
    }
    rememberCloudPayload(fetch.cloudBox, fetch.slot, result.payload);
    const bool stillValid = session_.hand.active && session_.handGeneration == fetch.handGeneration;
    if (!stillValid || fetch.cloudBox != session_.cloudPosition()) {
        context_.status = context_.text.get(stillValid ? TextId::SwapCancelledBoxChanged : TextId::SwapCancelled);
        return;
    }
    storage_.completeCloudSwap(fetch.slot, std::move(result.payload));
}

void CloudSyncController::beginRenameBox(std::uint16_t position, std::string name) {
    if (rename_.running()) {
        return;
    }
    if (!context_.signedIn()) {
        context_.status = context_.text.get(TextId::SignInAgain);
        return;
    }
    rename_.start([&api = context_.api, position, name = std::move(name), token = context_.account.accessToken]() {
        return RenameJob{position, api.renameBox(position, name, token)};
    });
}

void CloudSyncController::pollRenameBox() {
    RenameJob job;
    if (rename_.poll(job) && job.result.success) {
        session_.cloudBoxNames[job.position] = job.result.name;
    }
}
