#include "bank/DeviceCommitBackend.hpp"

#include "core/Logger.hpp"

#include <utility>

namespace {
RemoteOutcome toRemote(RequestOutcome outcome) {
    switch (outcome) {
        case RequestOutcome::Succeeded:
            return RemoteOutcome::Done;
        case RequestOutcome::Refused:
            return RemoteOutcome::Refused;
        case RequestOutcome::Unknown:
            break;
    }
    return RemoteOutcome::Unknown;
}

CloudSlot cloudSlot(const SlotRef& ref) {
    return {ref.cloudPosition(), ref.oneBasedSlot()};
}

bool validCloudSlot(CloudSlot slot) {
    return slot.boxPosition != 0 && slot.slot != 0 && slot.slot <= BoxSlotCount;
}
}

RemoteResult DeviceCommitBackend::moveCloud(const SlotRef& from, const SlotRef& to) {
    const MoveResult moved = api_.moveCloudPokemon(cloudSlot(from), cloudSlot(to), accessToken_);
    return {toRemote(moved.outcome), moved.message};
}

UploadBatchResult DeviceCommitBackend::uploadBatch(const std::vector<UploadItem>& items) {
    std::vector<UploadPokemon> batch;
    batch.reserve(items.size());
    for (const UploadItem& item : items) {
        batch.push_back({cloudSlot(item.destination), item.summary, item.payload, item.replaceOccupant});
    }
    const UploadResult sent = api_.uploadPokemon(batch, accessToken_);
    UploadBatchResult result{toRemote(sent.outcome), sent.message, {}};
    for (const UploadRejection& rejection : sent.rejected) {
        if (validCloudSlot(rejection.slot)) {
            result.rejected.push_back({SlotRef::cloud(static_cast<std::uint16_t>(rejection.slot.boxPosition - 1),
                                                      rejection.slot.slot - 1u), rejection.reason});
        }
    }
    return result;
}

std::optional<std::array<PokemonPayload, BoxSlotCount>> DeviceCommitBackend::readCloudBox(std::uint16_t boxKey) {
    BoxListResult listed = api_.listCloudBox(static_cast<std::uint16_t>(boxKey + 1), accessToken_);
    if (!listed.success) {
        return std::nullopt;
    }
    return std::move(listed.payloads);
}

RemoteResult DeviceCommitBackend::deleteCloud(const std::vector<SlotRef>& slots) {
    std::vector<CloudSlot> targets;
    targets.reserve(slots.size());
    for (const SlotRef& slot : slots) {
        targets.push_back(cloudSlot(slot));
    }
    const DeleteResult removed = api_.deleteCloudPokemonBatch(targets, accessToken_);
    if (!removed.unsupported) {
        return {toRemote(removed.outcome), removed.message};
    }
    for (const CloudSlot& target : targets) {
        const DeleteResult single = api_.deleteCloudPokemon(target, accessToken_);
        if (single.outcome != RequestOutcome::Succeeded) {
            return {toRemote(single.outcome), single.message};
        }
    }
    return {RemoteOutcome::Done, "Deleted."};
}

ClaimBatchResult DeviceCommitBackend::claimCloud(const std::vector<ClaimItem>& items) {
    std::vector<ClaimRequest> requests;
    requests.reserve(items.size());
    for (const ClaimItem& item : items) {
        requests.push_back({cloudSlot(item.slot), item.payload.data});
    }
    const ClaimResult claimed = api_.claimCloudPokemon(requests, accessToken_);
    ClaimBatchResult result{toRemote(claimed.outcome), claimed.message, {}};
    for (const ClaimState state : claimed.states) {
        result.claims.push_back(state == ClaimState::Claimed ? CloudClaim::Claimed
                                : state == ClaimState::Locked ? CloudClaim::Locked
                                                              : CloudClaim::Changed);
    }
    return result;
}

bool DeviceCommitBackend::writeLocal(const SlotRef& slot, const PokemonPayload& payload) {
    return slot.kind == SlotKind::Party ? save_.writePartyPokemon(slot.slot, payload)
                                        : save_.writePokemon(slot.box, slot.slot, payload);
}

bool DeviceCommitBackend::clearLocal(const SlotRef& slot) {
    return slot.kind == SlotKind::Party ? save_.clearPartySlot(slot.slot) : save_.clearSlot(slot.box, slot.slot);
}

bool DeviceCommitBackend::persistLocal(std::string& error, bool finalWrite) {
    return save_.writeSave(error, finalWrite);
}

void DeviceCommitBackend::discardLocal() {
    std::string error;
    if (!save_.discardUnsavedChanges(error)) {
        Logger::instance().error("commit: could not reset the in-memory save: " + error);
    }
}

void DeviceCommitBackend::log(const std::string& message) {
    Logger::instance().info(message);
}
