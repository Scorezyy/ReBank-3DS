#include "bank/StorageController.hpp"

#include "app/App.hpp"
#include "core/Logger.hpp"
#include "save/PayloadHash.hpp"

#include <utility>

namespace {
void logSlot(const std::string& action, const std::string& location, const PokemonSummary& mon,
             const PokemonPayload& payload) {
    Logger::instance().info(action + ": " + location + " species " + std::to_string(mon.species)
                            + " \"" + mon.nickname + "\" payload=" + payloadTag(payload.data)
                            + " (" + std::to_string(payload.data.size()) + " bytes)");
}
}

void StorageController::pickUp() {
    if (session_.hand.active) {
        return;
    }
    switch (session_.storagePane) {
        case StoragePane::Local:
            pickUpLocal();
            return;
        case StoragePane::Party:
            pickUpParty();
            return;
        case StoragePane::Cloud:
            pickUpCloud();
            return;
    }
}

void StorageController::pickUpLocal() {
    const PokemonSummary mon = session_.storage.pokemon(session_.focusedSlot);
    if (mon.species == 0) {
        app_.status_ = "This slot is empty.";
        return;
    }
    session_.hand.active = true;
    session_.hand.source = HandSource::Local;
    session_.hand.sourceIndex = session_.focusedSlot;
    session_.hand.sourceLocalBox = session_.localBox;
    session_.hand.summary = mon;
    session_.hand.payload = std::move(session_.localPayloads[session_.focusedSlot]);
    session_.hand.payloadKnown = !session_.hand.payload.data.empty();
    session_.localPayloads[session_.focusedSlot] = {};
    session_.storage.set(session_.focusedSlot, PokemonSummary{});
    ++session_.handGeneration;
    app_.status_ = mon.nickname + " picked up.";
    logSlot("pickUpLocal", "local box " + std::to_string(session_.localBox + 1) + " slot "
           + std::to_string(session_.focusedSlot + 1), mon, session_.hand.payload);
}

void StorageController::pickUpParty() {
    const PokemonSummary mon = session_.partyWorking.summaries[session_.focusedSlot];
    if (mon.species == 0) {
        app_.status_ = "This slot is empty.";
        return;
    }
    if (session_.partyMemberCount() <= 1) {
        app_.status_ = "Your team can't be empty.";
        return;
    }
    session_.hand.active = true;
    session_.hand.source = HandSource::Party;
    session_.hand.sourceIndex = session_.focusedSlot;
    session_.hand.summary = mon;
    session_.hand.payload = std::move(session_.partyWorking.payloads[session_.focusedSlot]);
    session_.hand.payloadKnown = !session_.hand.payload.data.empty();
    session_.partyWorking.payloads[session_.focusedSlot] = {};
    session_.partyWorking.summaries[session_.focusedSlot] = PokemonSummary{};
    ++session_.handGeneration;
    app_.status_ = mon.nickname + " picked up.";
    logSlot("pickUpParty", "party slot " + std::to_string(session_.focusedSlot + 1), mon, session_.hand.payload);
}

void StorageController::pickUpCloud() {
    if (session_.trashBoxActive) {
        const PokemonSummary mon = session_.trashBox.summaries()[session_.focusedSlot];
        if (mon.species == 0) {
            app_.status_ = "This slot is empty.";
            return;
        }
        session_.hand.active = true;
        session_.hand.source = HandSource::Cloud;
        session_.hand.sourceIndex = session_.focusedSlot;
        session_.hand.sourceTrash = true;
        session_.hand.summary = mon;
        session_.hand.payload = std::move(session_.trashBox.payloads()[session_.focusedSlot]);
        session_.hand.payloadKnown = !session_.hand.payload.data.empty();
        session_.trashBox.summaries()[session_.focusedSlot] = {};
        session_.trashBox.payloads()[session_.focusedSlot] = {};
        ++session_.handGeneration;
        app_.status_ = mon.nickname + " picked up.";
        logSlot("pickUpCloud (trash)", "trash box slot " + std::to_string(session_.focusedSlot + 1),
               mon, session_.hand.payload);
        return;
    }
    const PokemonSummary mon = session_.cloudPreview[session_.focusedSlot];
    if (mon.species == 0) {
        app_.status_ = "This slot is empty.";
        return;
    }
    if (app_.session_.accessToken.empty()) {
        app_.status_ = "Please sign in again.";
        return;
    }

    PokemonPayload payload;
    std::string payloadSource;
    if (!session_.pendingUploadPayloads[session_.focusedSlot].data.empty()) {
        payload = std::move(session_.pendingUploadPayloads[session_.focusedSlot]);
        session_.pendingUploadPayloads[session_.focusedSlot] = {};
        payloadSource = "pending";
    } else if (!session_.cachedCloudPayloads[session_.focusedSlot].data.empty()) {
        payload = session_.cachedCloudPayloads[session_.focusedSlot];
        payloadSource = "cached";
    } else {
        session_.hand.active = true;
        session_.hand.source = HandSource::Cloud;
        session_.hand.sourceIndex = session_.focusedSlot;
        session_.hand.sourceCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
        session_.hand.summary = mon;
        session_.hand.payload = {};
        session_.hand.payloadKnown = false;
        session_.cloudPreview[session_.focusedSlot] = {};
        ++session_.handGeneration;
        app_.status_ = mon.nickname + " picked up.";
        Logger::instance().info("pickUpCloud: bank " + std::to_string(session_.cloudBox + 1) + " slot "
                                + std::to_string(session_.focusedSlot + 1) + " species " + std::to_string(mon.species)
                                + " \"" + mon.nickname + "\" payload pending fetch");
        if (!app_.loadService_.running()) {
            app_.loadService_.pickupSlot = session_.focusedSlot;
            app_.loadService_.pickupCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
            app_.loadService_.pickupSummary = mon;
            app_.loadService_.pickupHandGeneration = session_.handGeneration;
            app_.loadService_.begin(LoadService::Operation::PickupCloud);
        }
        return;
    }

    session_.hand.active = true;
    session_.hand.source = HandSource::Cloud;
    session_.hand.sourceIndex = session_.focusedSlot;
    session_.hand.sourceCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
    session_.hand.summary = mon;
    session_.hand.payload = std::move(payload);
    session_.hand.payloadKnown = !session_.hand.payload.data.empty();
    session_.cloudPreview[session_.focusedSlot] = {};
    ++session_.handGeneration;
    app_.status_ = mon.nickname + " picked up.";
    logSlot("pickUpCloud (" + payloadSource + ")", "bank " + std::to_string(session_.cloudBox + 1) + " slot "
           + std::to_string(session_.focusedSlot + 1), mon, session_.hand.payload);
}

void StorageController::drop() {
    if (!session_.hand.active) {
        return;
    }
    if (!session_.hand.payloadKnown) {
        app_.status_ = "Still fetching " + session_.hand.summary.nickname + "...";
        return;
    }
    switch (session_.storagePane) {
        case StoragePane::Local:
            dropLocal();
            return;
        case StoragePane::Party:
            dropParty();
            return;
        case StoragePane::Cloud:
            dropCloud();
            return;
    }
}

void StorageController::dropLocal() {
    const std::uint8_t saveGen = session_.saveAdapter.gameGeneration();
    if (!session_.saveAdapter.canImportPokemon(session_.hand.payload.format, session_.hand.payload.data)) {
        app_.status_ = "Gen " + std::to_string(session_.hand.payload.format)
                  + " cannot enter Gen " + std::to_string(saveGen) + ".";
        return;
    }
    const bool occupied = session_.storage.pokemon(session_.focusedSlot).species != 0;
    if (occupied) {
        PokemonSummary occupantSummary = session_.storage.pokemon(session_.focusedSlot);
        PokemonPayload occupantPayload = std::move(session_.localPayloads[session_.focusedSlot]);
        SwapOrigin swapOrigin;
        swapOrigin.active = true;
        swapOrigin.source = session_.hand.source;
        swapOrigin.sourceIndex = session_.hand.sourceIndex;
        swapOrigin.sourceLocalBox = session_.hand.sourceLocalBox;
        swapOrigin.sourceCloudBox = session_.hand.sourceCloudBox;
        swapOrigin.sourceTrash = session_.hand.sourceTrash;
        swapOrigin.summary = session_.hand.summary;
        swapOrigin.payload = session_.hand.payload;
        const std::string location = "local box " + std::to_string(session_.localBox + 1) + " slot "
                                     + std::to_string(session_.focusedSlot + 1);
        Logger::instance().info("dropLocal swap: " + location + " incoming species "
                                + std::to_string(session_.hand.summary.species) + " payload="
                                + payloadTag(session_.hand.payload.data) + " ; outgoing species "
                                + std::to_string(occupantSummary.species) + " payload="
                                + payloadTag(occupantPayload.data));
        session_.storage.set(session_.focusedSlot, session_.hand.summary);
        session_.localPayloads[session_.focusedSlot] = session_.hand.payload;
        session_.hand.summary = std::move(occupantSummary);
        session_.hand.payload = std::move(occupantPayload);
        session_.hand.source = HandSource::Local;
        session_.hand.sourceIndex = session_.focusedSlot;
        session_.hand.sourceLocalBox = session_.localBox;
        session_.hand.sourceTrash = false;
        session_.hand.payloadKnown = !session_.hand.payload.data.empty();
        session_.hand.swapOrigin = std::move(swapOrigin);
        ++session_.handGeneration;
        app_.status_ = session_.hand.summary.nickname + " swapped.";
        return;
    }
    session_.storage.set(session_.focusedSlot, session_.hand.summary);
    session_.localPayloads[session_.focusedSlot] = session_.hand.payload;
    app_.status_ = session_.hand.summary.nickname + " placed.";
    logSlot("dropLocal placed", "local box " + std::to_string(session_.localBox + 1) + " slot "
           + std::to_string(session_.focusedSlot + 1), session_.hand.summary, session_.hand.payload);
    session_.hand = Hand{};
    ++session_.handGeneration;
}

void StorageController::dropParty() {
    const std::uint8_t saveGen = session_.saveAdapter.gameGeneration();
    if (!session_.saveAdapter.canImportPokemon(session_.hand.payload.format, session_.hand.payload.data)) {
        app_.status_ = "Gen " + std::to_string(session_.hand.payload.format)
                  + " cannot enter Gen " + std::to_string(saveGen) + ".";
        return;
    }
    const bool occupied = session_.partyWorking.summaries[session_.focusedSlot].species != 0;
    if (occupied) {
        PokemonSummary occupantSummary = session_.partyWorking.summaries[session_.focusedSlot];
        PokemonPayload occupantPayload = std::move(session_.partyWorking.payloads[session_.focusedSlot]);
        Logger::instance().info("dropParty swap: party slot " + std::to_string(session_.focusedSlot + 1)
                                + " incoming species " + std::to_string(session_.hand.summary.species)
                                + " payload=" + payloadTag(session_.hand.payload.data) + " ; outgoing species "
                                + std::to_string(occupantSummary.species) + " payload="
                                + payloadTag(occupantPayload.data));
        session_.partyWorking.summaries[session_.focusedSlot] = session_.hand.summary;
        session_.partyWorking.payloads[session_.focusedSlot] = session_.hand.payload;
        session_.hand.summary = std::move(occupantSummary);
        session_.hand.payload = std::move(occupantPayload);
        session_.hand.source = HandSource::Party;
        session_.hand.sourceIndex = session_.focusedSlot;
        session_.hand.payloadKnown = !session_.hand.payload.data.empty();
        ++session_.handGeneration;
        app_.status_ = session_.hand.summary.nickname + " swapped.";
        return;
    }
    session_.partyWorking.summaries[session_.focusedSlot] = session_.hand.summary;
    session_.partyWorking.payloads[session_.focusedSlot] = session_.hand.payload;
    app_.status_ = session_.hand.summary.nickname + " placed.";
    logSlot("dropParty placed", "party slot " + std::to_string(session_.focusedSlot + 1),
           session_.hand.summary, session_.hand.payload);
    session_.hand = Hand{};
    ++session_.handGeneration;
}

void StorageController::dropCloud() {
    if (session_.trashBoxActive) {
        const bool trashOccupied = session_.trashBox.summaries()[session_.focusedSlot].species != 0;
        if (trashOccupied) {
            PokemonSummary occupantSummary = session_.trashBox.summaries()[session_.focusedSlot];
            PokemonPayload occupantPayload = std::move(session_.trashBox.payloads()[session_.focusedSlot]);
            SwapOrigin swapOrigin;
            swapOrigin.active = true;
            swapOrigin.source = session_.hand.source;
            swapOrigin.sourceIndex = session_.hand.sourceIndex;
            swapOrigin.sourceLocalBox = session_.hand.sourceLocalBox;
            swapOrigin.sourceCloudBox = session_.hand.sourceCloudBox;
            swapOrigin.sourceTrash = session_.hand.sourceTrash;
            swapOrigin.summary = session_.hand.summary;
            swapOrigin.payload = session_.hand.payload;
            Logger::instance().info("dropCloud swap (trash): slot " + std::to_string(session_.focusedSlot + 1)
                                    + " incoming species " + std::to_string(session_.hand.summary.species)
                                    + " payload=" + payloadTag(session_.hand.payload.data) + " ; outgoing species "
                                    + std::to_string(occupantSummary.species) + " payload="
                                    + payloadTag(occupantPayload.data));
            session_.trashBox.summaries()[session_.focusedSlot] = session_.hand.summary;
            session_.trashBox.payloads()[session_.focusedSlot] = session_.hand.payload;
            session_.hand.summary = std::move(occupantSummary);
            session_.hand.payload = std::move(occupantPayload);
            session_.hand.source = HandSource::Cloud;
            session_.hand.sourceIndex = session_.focusedSlot;
            session_.hand.sourceTrash = true;
            session_.hand.payloadKnown = !session_.hand.payload.data.empty();
            session_.hand.swapOrigin = std::move(swapOrigin);
            ++session_.handGeneration;
            app_.status_ = session_.hand.summary.nickname + " swapped.";
            return;
        }
        session_.trashBox.summaries()[session_.focusedSlot] = session_.hand.summary;
        session_.trashBox.payloads()[session_.focusedSlot] = session_.hand.payload;
        app_.status_ = session_.hand.summary.nickname + " placed.";
        logSlot("dropCloud placed (trash)", "trash box slot " + std::to_string(session_.focusedSlot + 1),
               session_.hand.summary, session_.hand.payload);
        session_.hand = Hand{};
        ++session_.handGeneration;
        return;
    }
    const bool occupied = session_.cloudPreview[session_.focusedSlot].species != 0;
    if (occupied) {
        PokemonPayload occupantPayload;
        std::string payloadSource;
        if (!session_.pendingUploadPayloads[session_.focusedSlot].data.empty()) {
            occupantPayload = std::move(session_.pendingUploadPayloads[session_.focusedSlot]);
            session_.pendingUploadPayloads[session_.focusedSlot] = {};
            payloadSource = "pending";
        } else if (!session_.cachedCloudPayloads[session_.focusedSlot].data.empty()) {
            occupantPayload = session_.cachedCloudPayloads[session_.focusedSlot];
            payloadSource = "cached";
        } else if (!app_.session_.accessToken.empty()) {
            if (app_.loadService_.running()) {
                return;
            }
            app_.loadService_.pickupSlot = session_.focusedSlot;
            app_.loadService_.pickupCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
            app_.loadService_.pickupSummary = session_.cloudPreview[session_.focusedSlot];
            app_.loadService_.pickupHandGeneration = session_.handGeneration;
            app_.status_ = "Fetching occupant...";
            app_.loadService_.begin(LoadService::Operation::SwapCloud);
            return;
        } else {
            app_.status_ = "Please sign in again.";
            return;
        }
        PokemonSummary occupantSummary = session_.cloudPreview[session_.focusedSlot];
        Logger::instance().info("dropCloud swap: bank " + std::to_string(session_.cloudBox + 1) + " slot "
                                + std::to_string(session_.focusedSlot + 1)
                                + " incoming species " + std::to_string(session_.hand.summary.species)
                                + " payload=" + payloadTag(session_.hand.payload.data) + " ; outgoing species "
                                + std::to_string(occupantSummary.species) + " payload="
                                + payloadTag(occupantPayload.data) + " (source=" + payloadSource + ")");
        session_.cloudPreview[session_.focusedSlot] = session_.hand.summary;
        session_.pendingUploadPayloads[session_.focusedSlot] = session_.hand.payload;
        session_.hand.summary = std::move(occupantSummary);
        session_.hand.payload = std::move(occupantPayload);
        session_.hand.source = HandSource::Cloud;
        session_.hand.sourceIndex = session_.focusedSlot;
        session_.hand.sourceCloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
        session_.hand.sourceTrash = false;
        session_.hand.payloadKnown = !session_.hand.payload.data.empty();
        ++session_.handGeneration;
        app_.status_ = session_.hand.summary.nickname + " swapped.";
        return;
    }
    session_.cloudPreview[session_.focusedSlot] = session_.hand.summary;
    PokemonPayload payload = session_.hand.payload;
    session_.pendingUploadPayloads[session_.focusedSlot] = std::move(payload);
    ++session_.handGeneration;
    app_.status_ = session_.hand.summary.nickname + " placed.";
    logSlot("dropCloud placed", "bank " + std::to_string(session_.cloudBox + 1) + " slot "
           + std::to_string(session_.focusedSlot + 1), session_.hand.summary,
           session_.pendingUploadPayloads[session_.focusedSlot]);
    session_.hand = Hand{};
}

LocalBoxDraft& StorageController::localDraftForWrite(std::size_t box) {
    auto draftIt = session_.localDrafts.find(box);
    if (draftIt != session_.localDrafts.end()) {
        return draftIt->second;
    }
    const auto baselineIt = session_.localBaselines.find(box);
    LocalBoxDraft draft = baselineIt != session_.localBaselines.end() ? baselineIt->second : LocalBoxDraft{};
    return session_.localDrafts.emplace(box, std::move(draft)).first->second;
}

void StorageController::restorePokemon(HandSource source, std::size_t sourceIndex,
                                       std::size_t sourceLocalBox, std::uint16_t sourceCloudBox,
                                       bool sourceTrash, const PokemonSummary& summary,
                                       const PokemonPayload& payload) {
    switch (source) {
        case HandSource::Local:
            if (sourceLocalBox == session_.localBox) {
                session_.storage.set(sourceIndex, summary);
                session_.localPayloads[sourceIndex] = payload;
            } else {
                LocalBoxDraft& draft = localDraftForWrite(sourceLocalBox);
                draft.summaries[sourceIndex] = summary;
                draft.payloads[sourceIndex] = payload;
            }
            return;
        case HandSource::Party:
            session_.partyWorking.summaries[sourceIndex] = summary;
            session_.partyWorking.payloads[sourceIndex] = payload;
            return;
        case HandSource::Cloud:
            if (sourceTrash) {
                session_.trashBox.summaries()[sourceIndex] = summary;
                session_.trashBox.payloads()[sourceIndex] = payload;
                return;
            }
            if (sourceCloudBox == 0) {
                return;
            }
            break;
    }

    const auto boxKey = static_cast<std::uint16_t>(sourceCloudBox - 1);
    if (auto boxIt = session_.cloudBoxes.find(boxKey); boxIt != session_.cloudBoxes.end()) {
        boxIt->second.summaries[sourceIndex] = summary;
        boxIt->second.pending[sourceIndex] = {};
        boxIt->second.payloads[sourceIndex] = payload;
    }
    if (boxKey == static_cast<std::uint16_t>(session_.cloudBox)) {
        session_.cloudPreview[sourceIndex] = summary;
        session_.cachedCloudPayloads[sourceIndex] = payload;
        session_.pendingUploadPayloads[sourceIndex] = {};
    }
}

void StorageController::returnHand() {
    if (!session_.hand.active) {
        return;
    }

    Hand hand = std::move(session_.hand);
    restorePokemon(hand.source, hand.sourceIndex, hand.sourceLocalBox, hand.sourceCloudBox,
                   hand.sourceTrash, hand.summary, hand.payload);
    if (hand.swapOrigin.active) {
        restorePokemon(hand.swapOrigin.source, hand.swapOrigin.sourceIndex,
                       hand.swapOrigin.sourceLocalBox, hand.swapOrigin.sourceCloudBox,
                       hand.swapOrigin.sourceTrash, hand.swapOrigin.summary, hand.swapOrigin.payload);
        Logger::instance().info("returnHand: undone swap and restored incoming Pokemon to its source slot");
    }

    app_.status_ = "Returned to slot " + std::to_string(hand.sourceIndex + 1) + ".";
    Logger::instance().info("returnHand: source=" + std::to_string(static_cast<int>(hand.source))
                            + " slot " + std::to_string(hand.sourceIndex + 1)
                            + " species " + std::to_string(hand.summary.species)
                            + " \"" + hand.summary.nickname + "\" payload="
                            + payloadTag(hand.payload.data));
    session_.hand = Hand{};
    ++session_.handGeneration;
}

bool StorageController::localBoxDiffers(const LocalBoxDraft& a, const LocalBoxDraft& b, std::size_t slot) const {
    return a.summaries[slot].species != b.summaries[slot].species
        || a.summaries[slot].nickname != b.summaries[slot].nickname
        || a.payloads[slot].data != b.payloads[slot].data;
}

bool StorageController::partySlotDiffers(std::size_t slot) const {
    const PokemonSummary& working = session_.partyWorking.summaries[slot];
    const PokemonSummary& baseline = session_.partyBaseline.summaries[slot];
    return working.species != baseline.species
        || working.nickname != baseline.nickname
        || session_.partyWorking.payloads[slot].data != session_.partyBaseline.payloads[slot].data;
}

bool StorageController::hasPendingChanges(bool verbose) const {
    for (const auto& [box, draft] : session_.localDrafts) {
        auto it = session_.localBaselines.find(box);
        if (it == session_.localBaselines.end()) {
            if (verbose) {
                Logger::instance().info("hasPendingChanges: localDrafts_ box "
                                        + std::to_string(box) + " has no baseline");
            }
            return true;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (localBoxDiffers(draft, it->second, slot)) {
                if (verbose) {
                    Logger::instance().info("hasPendingChanges: localDrafts_ box "
                                            + std::to_string(box) + " slot " + std::to_string(slot)
                                            + " differs from baseline");
                }
                return true;
            }
        }
    }
    if (session_.localBox < session_.saveAdapter.boxCount()) {
        auto it = session_.localBaselines.find(session_.localBox);
        if (it != session_.localBaselines.end()) {
            for (std::size_t slot = 0; slot < 30; ++slot) {
                if (session_.storage.pokemon(slot).species != it->second.summaries[slot].species
                    || session_.storage.pokemon(slot).nickname != it->second.summaries[slot].nickname
                    || session_.localPayloads[slot].data != it->second.payloads[slot].data) {
                    if (verbose) {
                        Logger::instance().info("hasPendingChanges: local box "
                                                + std::to_string(session_.localBox) + " slot " + std::to_string(slot)
                                                + " differs from baseline (species "
                                                + std::to_string(session_.storage.pokemon(slot).species) + " vs "
                                                + std::to_string(it->second.summaries[slot].species)
                                                + ", payload " + std::to_string(session_.localPayloads[slot].data.size())
                                                + " vs " + std::to_string(it->second.payloads[slot].data.size()) + " bytes)");
                    }
                    return true;
                }
            }
        }
    }
    for (const auto& [box, draft] : session_.cloudBoxes) {
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (draft.summaries[slot].species != draft.baseline[slot].species
                || draft.summaries[slot].nickname != draft.baseline[slot].nickname
                || !draft.pending[slot].data.empty()) {
                if (verbose) {
                    Logger::instance().info("hasPendingChanges: cloud box "
                                            + std::to_string(box) + " slot " + std::to_string(slot)
                                            + " differs from baseline or has pending upload");
                }
                return true;
            }
        }
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (!session_.pendingUploadPayloads[slot].data.empty()) {
            if (verbose) {
                Logger::instance().info("hasPendingChanges: pendingUploadPayloads_ slot "
                                        + std::to_string(slot) + " non-empty");
            }
            return true;
        }
    }
    if (!session_.trashBox.empty()) {
        if (verbose) {
            Logger::instance().info("hasPendingChanges: trash box holds Pokemon awaiting deletion");
        }
        return true;
    }
    for (std::size_t slot = 0; slot < 6; ++slot) {
        if (partySlotDiffers(slot)) {
            if (verbose) {
                const auto& working = session_.partyWorking.summaries[slot];
                const auto& baseline = session_.partyBaseline.summaries[slot];
                Logger::instance().info("hasPendingChanges: party slot " + std::to_string(slot)
                                        + " differs from baseline (species "
                                        + std::to_string(working.species) + " vs "
                                        + std::to_string(baseline.species)
                                        + ", payload " + std::to_string(session_.partyWorking.payloads[slot].data.size())
                                        + " vs " + std::to_string(session_.partyBaseline.payloads[slot].data.size()) + " bytes)");
            }
            return true;
        }
    }
    return false;
}

void StorageController::loadLocalBox() {
    session_.localBoxName = session_.saveAdapter.boxName(session_.localBox);
    session_.storagePane = StoragePane::Local;
    session_.focusedSlot = 0;

    if (session_.localBaselines.find(session_.localBox) == session_.localBaselines.end()) {
        LocalBoxDraft baseline;
        const BoxRead read = session_.saveAdapter.readBoxFull(session_.localBox);
        baseline.summaries = read.summaries;
        baseline.payloads = read.payloads;
        session_.localBaselines[session_.localBox] = std::move(baseline);
    }

    auto draftIt = session_.localDrafts.find(session_.localBox);
    if (draftIt != session_.localDrafts.end()) {
        session_.storage.load(draftIt->second.summaries);
        session_.localPayloads = draftIt->second.payloads;
    } else {
        const LocalBoxDraft& baseline = session_.localBaselines[session_.localBox];
        session_.storage.load(baseline.summaries);
        session_.localPayloads = baseline.payloads;
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (session_.storage.pokemon(slot).species != 0) {
            session_.focusedSlot = slot;
            break;
        }
    }
    Logger::instance().info("Local box loaded: " + std::to_string(session_.localBox + 1));
}

void StorageController::loadTrashBox() {
    session_.storagePane = StoragePane::Cloud;
    session_.cloudNameFocused = false;
    session_.focusedSlot = 0;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (session_.trashBox.summaries()[slot].species != 0) {
            session_.focusedSlot = slot;
            break;
        }
    }
    Logger::instance().info("Trash box loaded (" + std::to_string(session_.trashBox.count()) + " occupied)");
}

void StorageController::emptyTrashBox() {
    Logger::instance().info("emptyTrashBox: permanently discarding " + std::to_string(session_.trashBox.count())
                            + " Pokemon");
    session_.trashBox.reset();
}

void StorageController::persistLocalDraft() {
    auto baselineIt = session_.localBaselines.find(session_.localBox);
    if (baselineIt == session_.localBaselines.end()) {
        return;
    }
    LocalBoxDraft current;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        current.summaries[slot] = session_.storage.pokemon(slot);
        current.payloads[slot] = session_.localPayloads[slot];
    }
    bool differs = false;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (localBoxDiffers(current, baselineIt->second, slot)) {
            differs = true;
            break;
        }
    }
    if (differs) {
        session_.localDrafts[session_.localBox] = std::move(current);
    } else {
        session_.localDrafts.erase(session_.localBox);
    }
}

void StorageController::persistCloudDraft() {
    const auto boxKey = static_cast<std::uint16_t>(session_.cloudBox);
    auto& draft = session_.cloudBoxes[boxKey];
    draft.summaries = session_.cloudPreview;
    draft.pending = session_.pendingUploadPayloads;
}

void StorageController::refreshCloudBox(bool keepPreviousPreview) {
    const auto boxKey = static_cast<std::uint16_t>(session_.cloudBox);
    if (app_.session_.accessToken.empty()) {
        session_.cloudPreview.fill({});
        session_.pendingUploadPayloads = {};
        return;
    }

    auto it = session_.cloudBoxes.find(boxKey);
    if (it == session_.cloudBoxes.end()) {
        if (!keepPreviousPreview) {
            session_.cloudPreview.fill({});
            session_.pendingUploadPayloads = {};
        }
        session_.cachedCloudPayloads = {};
        session_.payloadPrefetchFailed = {};
        app_.status_.clear();
        app_.loadService_.cloudBoxKey = boxKey;
        app_.loadService_.begin(LoadService::Operation::CloudBox);
        return;
    }
    session_.cloudPreview = it->second.summaries;
    session_.pendingUploadPayloads = it->second.pending;
    session_.cachedCloudPayloads = it->second.payloads;
    session_.payloadPrefetchFailed = {};
    std::size_t occupied = 0;
    for (const auto& mon : session_.cloudPreview) {
        if (mon.species != 0) {
            ++occupied;
        }
    }
    Logger::instance().info("Cloud box " + std::to_string(session_.cloudBox + 1)
                            + " loaded (" + std::to_string(occupied) + " occupied)");
}

void StorageController::discardPendingChanges() {
    const StoragePane previousPane = session_.storagePane;
    session_.hand = Hand{};
    session_.localDrafts.clear();
    session_.localBaselines.clear();
    for (auto& [key, draft] : session_.cloudBoxes) {
        draft.summaries = draft.baseline;
        draft.pending = {};
    }
    session_.pendingUploadPayloads = {};
    session_.cachedCloudPayloads = {};
    session_.payloadPrefetchFailed = {};
    session_.partyWorking = session_.partyBaseline;
    session_.trashBox.reset();
    session_.trashBoxActive = false;
    loadLocalBox();
    session_.storagePane = previousPane;
    refreshCloudBox();
    app_.status_ = "Pending changes discarded.";
    Logger::instance().info("Pending storage changes discarded, cloudBox="
                            + std::to_string(session_.cloudBox + 1) + " loaderRunning="
                            + std::to_string(app_.loadService_.running()));
}

void StorageController::initializeFromOpenedGame(LoadService::OpenGameResult& result) {
    session_.trashBox.reset();
    session_.trashBoxActive = false;
    session_.trashConfirmVisible = false;
    session_.saveSummary = std::move(result.save);
    session_.localBox = result.localBox;
    session_.cloudBox = 0;
    session_.localBoxName = std::move(result.localBoxName);
    session_.localBaselines.clear();
    session_.localDrafts.clear();
    LocalBoxDraft baseline;
    baseline.summaries = std::move(result.localPokemon);
    baseline.payloads = std::move(result.localPayloads);
    session_.localBaselines[session_.localBox] = std::move(baseline);
    session_.storage.load(session_.localBaselines[session_.localBox].summaries);
    session_.localPayloads = session_.localBaselines[session_.localBox].payloads;
    session_.partyBaseline.summaries = result.localParty;
    session_.partyBaseline.payloads = result.localPartyPayloads;
    session_.partyWorking = session_.partyBaseline;
    session_.storagePane = StoragePane::Local;
    session_.focusedSlot = 0;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (session_.storage.pokemon(slot).species != 0) {
            session_.focusedSlot = slot;
            break;
        }
    }
    session_.cloudBoxNames.clear();
    for (const auto& entry : app_.cloudBoxNamesCache_) {
        session_.cloudBoxNames[entry.position] = entry.name;
    }

    const auto existingBox0 = session_.cloudBoxes.find(0);
    if (existingBox0 != session_.cloudBoxes.end()) {
        session_.cloudPreview = existingBox0->second.summaries;
        session_.cachedCloudPayloads = existingBox0->second.payloads;
        app_.status_.clear();
    } else if (app_.cloudBoxCache_.success) {
        CloudBoxDraft cloud;
        cloud.baseline = app_.cloudBoxCache_.pokemon;
        cloud.summaries = app_.cloudBoxCache_.pokemon;
        cloud.payloads = app_.cloudBoxCache_.payloads;
        session_.cloudBoxes[0] = cloud;
        session_.cloudPreview = cloud.summaries;
        session_.cachedCloudPayloads = cloud.payloads;
        app_.status_.clear();
    } else {
        session_.cloudPreview.fill({});
        session_.cachedCloudPayloads = {};
        app_.status_.clear();
    }
    app_.cloudBoxCache_ = {};
    session_.pendingUploadPayloads = {};
    session_.payloadPrefetchFailed = {};
    session_.hand = Hand{};
}

void StorageController::reset() {
    session_.saveAdapter.close();
    session_.saveSummary = {};
    session_.cloudBoxes.clear();
    session_.cloudPrefetchCooldownUntil.clear();
    session_.localBaselines.clear();
    session_.localDrafts.clear();
    session_.cloudPreview.fill({});
    session_.partyBaseline = PartyDraft{};
    session_.partyWorking = PartyDraft{};
    session_.trashBox.reset();
    session_.trashBoxActive = false;
    session_.trashConfirmVisible = false;
}
