#include "bank/CommitService.hpp"

#include "app/App.hpp"
#include "core/Logger.hpp"

#include <algorithm>

namespace {
// Parses "Bank X, Slot Y: reason" (X is omitted for non-batch uploads) out
// of a server error message. Returns false if no slot could be found, which
// means the failure can't be attributed to one specific Pokemon.
bool parseBankSlot(const std::string& message, std::uint16_t& bank, std::uint8_t& slot) {
    bank = 0;
    const std::size_t bankMarker = message.find("Bank ");
    if (bankMarker != std::string::npos) {
        std::size_t index = bankMarker + 5;
        while (index < message.size() && message[index] >= '0' && message[index] <= '9') {
            bank = static_cast<std::uint16_t>(bank * 10 + (message[index] - '0'));
            ++index;
        }
    }
    const std::size_t marker = message.find("Slot ");
    if (marker == std::string::npos) {
        return false;
    }
    const std::size_t begin = marker + 5;
    std::size_t index = begin;
    slot = 0;
    while (index < message.size() && message[index] >= '0' && message[index] <= '9') {
        slot = static_cast<std::uint8_t>(slot * 10 + (message[index] - '0'));
        ++index;
    }
    return index != begin;
}

std::string reasonAfterSlot(const std::string& message) {
    const std::size_t colon = message.find(':', message.find("Slot "));
    if (colon == std::string::npos || colon + 1 >= message.size()) {
        return message;
    }
    std::string reason = message.substr(colon + 1);
    while (!reason.empty() && reason.front() == ' ') {
        reason.erase(reason.begin());
    }
    return reason;
}
}

void CommitService::advanceProgress() {
    ++completedSteps_;
    const int percent = totalSteps_ == 0
        ? 100
        : static_cast<int>((completedSteps_ * 100) / totalSteps_);
    progress_.store(percent, std::memory_order_release);
}

void CommitService::begin() {
    if (job_.running()) {
        return;
    }
    if (app_.session_.accessToken.empty()) {
        app_.status_ = "Please sign in again.";
        return;
    }
    storage_.persistLocalDraft();
    storage_.persistCloudDraft();
    phase_.store(0, std::memory_order_release);
    progress_.store(0, std::memory_order_release);
    app_.status_ = "Committing changes...";
    if (!job_.start([this]() { runCommit(); })) {
        app_.status_ = "Could not start the commit.";
    }
}

void CommitService::requestWhenIdle() {
    requested_ = true;
    app_.status_ = "Committing changes...";
}

void CommitService::pumpRequest() {
    if (!requested_) {
        return;
    }
    if (app_.loadService_.running()) {
        return;
    }
    requested_ = false;
    if (session_.hand.active) {
        app_.status_ = "Drop the Pokemon first.";
        return;
    }
    if (!storage_.hasPendingChanges()) {
        app_.status_ = "Nothing to commit.";
        return;
    }
    begin();
}

void CommitService::collectCloudChanges(std::vector<UploadPokemon>& uploads, std::vector<BankSlot>& deletes) {
    for (const auto& [boxKey, draft] : session_.cloudBoxes) {
        const auto boxPosition = static_cast<std::uint16_t>(boxKey + 1);
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool initHas = draft.baseline[slot].species != 0;
            const bool nowHas = draft.summaries[slot].species != 0;
            const bool same = draft.summaries[slot].species == draft.baseline[slot].species
                && draft.summaries[slot].nickname == draft.baseline[slot].nickname;
            if (nowHas && (!same || !draft.pending[slot].data.empty())) {
                const auto& payload = draft.pending[slot];
                if (payload.data.empty()) {
                    Logger::instance().error("Commit: staged Pokemon missing payload, skipping slot "
                                             + std::to_string(slot + 1));
                    continue;
                }
                uploads.push_back(UploadPokemon{
                    boxPosition,
                    static_cast<std::uint8_t>(slot + 1),
                    payload.format,
                    payload.data,
                    draft.summaries[slot].species,
                    draft.summaries[slot].nickname,
                    draft.summaries[slot].trainerName,
                    draft.summaries[slot].level,
                    draft.summaries[slot].gameCode,
                    draft.summaries[slot].shiny,
                    draft.summaries[slot].heldItem
                });
            }
            if (initHas && !nowHas) {
                deletes.emplace_back(boxPosition, static_cast<std::uint8_t>(slot + 1));
            }
        }
    }
}

std::size_t CommitService::countLocalChanges() const {
    std::size_t count = 0;
    for (const auto& [boxKey, draft] : session_.localDrafts) {
        const auto baselineIt = session_.localBaselines.find(boxKey);
        if (baselineIt == session_.localBaselines.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool sameSummary = draft.summaries[slot].species == baselineIt->second.summaries[slot].species
                && draft.summaries[slot].nickname == baselineIt->second.summaries[slot].nickname;
            const bool samePayload = draft.payloads[slot].data == baselineIt->second.payloads[slot].data;
            if (!sameSummary || !samePayload) {
                ++count;
            }
        }
    }
    return count;
}

std::size_t CommitService::countPartyChanges() const {
    std::size_t count = 0;
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto& working = session_.partyWorking.summaries[slot];
        const auto& baseline = session_.partyBaseline.summaries[slot];
        const bool sameSummary = working.species == baseline.species && working.nickname == baseline.nickname;
        const bool samePayload = session_.partyWorking.payloads[slot].data == session_.partyBaseline.payloads[slot].data;
        if (!sameSummary || !samePayload) {
            ++count;
        }
    }
    return count;
}

void CommitService::recordSkippedUpload(const UploadPokemon& upload, const std::string& reason) {
    result_.skipped.push_back(CommitSkippedItem{
        upload.nickname.empty() ? "Pokemon #" + std::to_string(upload.species) : upload.nickname,
        "Bank " + std::to_string(upload.boxPosition) + "  |  Slot " + std::to_string(upload.slot),
        reason
    });
    if (result_.problemPokemon.empty()) {
        result_.problemPokemon = result_.skipped.back().nickname;
        result_.problemLocation = result_.skipped.back().location;
        result_.problemReason = reason;
    }
}

std::vector<UploadPokemon> CommitService::runUploads(std::vector<UploadPokemon> uploads) {
    std::vector<UploadPokemon> uploaded;
    if (uploads.empty()) {
        return uploaded;
    }
    phase_.store(2, std::memory_order_release);
    while (!uploads.empty()) {
        const std::size_t batchSize = std::min<std::size_t>(uploads.size(), 30);
        std::vector<UploadPokemon> batch(uploads.begin(), uploads.begin() + batchSize);
        UploadResult ur = app_.api_.uploadPokemon(batch, app_.session_.accessToken);
        if (ur.success) {
            result_.uploads += ur.storedCount;
            uploaded.insert(uploaded.end(), batch.begin(), batch.end());
            uploads.erase(uploads.begin(), uploads.begin() + batchSize);
            advanceProgress();
            continue;
        }

        std::uint16_t badBank = 0;
        std::uint8_t badSlot = 0;
        const std::size_t before = uploads.size();
        if (parseBankSlot(ur.message, badBank, badSlot)) {
            const std::string reason = reasonAfterSlot(ur.message);
            const auto bad = std::find_if(batch.begin(), batch.end(), [&](const UploadPokemon& u) {
                return u.slot == badSlot && (badBank == 0 || u.boxPosition == badBank);
            });
            if (bad != batch.end()) {
                recordSkippedUpload(*bad, reason);
            }
            uploads.erase(std::remove_if(uploads.begin(), uploads.end(), [&](const UploadPokemon& u) {
                return u.slot == badSlot && (badBank == 0 || u.boxPosition == badBank);
            }), uploads.end());
        }
        if (uploads.size() == before) {
            // Couldn't identify (or remove) the offending Pokemon - nothing in
            // this batch has been persisted, so drop the whole batch rather
            // than retry it unchanged forever.
            for (const auto& item : batch) {
                recordSkippedUpload(item, ur.message);
            }
            uploads.erase(uploads.begin(), uploads.begin() + batchSize);
        }
        advanceProgress();
    }
    return uploaded;
}

void CommitService::commitCloudUploadBaseline(const std::vector<UploadPokemon>& uploaded) {
    for (const auto& item : uploaded) {
        if (item.boxPosition == 0) {
            continue;
        }
        const auto boxKey = static_cast<std::uint16_t>(item.boxPosition - 1);
        auto it = session_.cloudBoxes.find(boxKey);
        if (it == session_.cloudBoxes.end() || item.slot == 0 || item.slot > 30) {
            continue;
        }
        const std::size_t slot = static_cast<std::size_t>(item.slot - 1);
        // The upload really happened on the server - lock it in immediately
        // so a later, unrelated failure in this same commit can never cause
        // discardPendingChanges() to revert this slot back to "not uploaded"
        // while a real copy already exists on the server.
        it->second.baseline[slot] = it->second.summaries[slot];
        it->second.pending[slot] = {};
    }
}

void CommitService::runLocalWrites(std::vector<LocalWriteVerification>& verifications, bool& anyWrite, bool& anyFailure,
                                    const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads) {
    for (const auto& [boxKey, draft] : session_.localDrafts) {
        auto baselineIt = session_.localBaselines.find(boxKey);
        if (baselineIt == session_.localBaselines.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool sameSummary = draft.summaries[slot].species == baselineIt->second.summaries[slot].species
                && draft.summaries[slot].nickname == baselineIt->second.summaries[slot].nickname;
            const bool samePayload = draft.payloads[slot].data == baselineIt->second.payloads[slot].data;
            if (sameSummary && samePayload) {
                continue;
            }
            if (draft.summaries[slot].species == 0) {
                const auto& vacatedPayload = baselineIt->second.payloads[slot].data;
                const bool stillPendingElsewhere = !vacatedPayload.empty() && std::any_of(
                    unresolvedPayloads.begin(), unresolvedPayloads.end(),
                    [&](const std::vector<std::uint8_t>& payload) { return payload == vacatedPayload; });
                if (stillPendingElsewhere) {
                    Logger::instance().warning("Commit: keeping local box " + std::to_string(boxKey + 1)
                                               + " slot " + std::to_string(slot + 1)
                                               + " in place - its cloud upload was rejected");
                    result_.skipped.push_back(CommitSkippedItem{
                        baselineIt->second.summaries[slot].nickname.empty()
                            ? "Pokemon #" + std::to_string(baselineIt->second.summaries[slot].species)
                            : baselineIt->second.summaries[slot].nickname,
                        "Local box " + std::to_string(boxKey + 1) + "  |  Slot " + std::to_string(slot + 1),
                        "Its move to the cloud was rejected, so it was kept here."
                    });
                    anyFailure = true;
                    continue;
                }
                if (!session_.saveAdapter.clearSlot(boxKey, slot)) {
                    Logger::instance().warning("Commit: could not clear local slot " + std::to_string(slot + 1)
                                               + ", leaving it as-is");
                    anyFailure = true;
                    continue;
                }
            } else if (!draft.payloads[slot].data.empty()) {
                if (!session_.saveAdapter.writePokemon(boxKey, slot, draft.payloads[slot].format, draft.payloads[slot].data)) {
                    result_.skipped.push_back(CommitSkippedItem{
                        draft.summaries[slot].nickname.empty()
                            ? "Pokemon #" + std::to_string(draft.summaries[slot].species)
                            : draft.summaries[slot].nickname,
                        "Local box " + std::to_string(boxKey + 1) + "  |  Slot " + std::to_string(slot + 1),
                        "Incompatible generation for this save."
                    });
                    anyFailure = true;
                    continue;
                }
                verifications.push_back({boxKey, slot, draft.summaries[slot].species});
                ++result_.downloads;
            }
            anyWrite = true;
            advanceProgress();
        }
    }
}

void CommitService::runPartyWrites(std::vector<std::size_t>& slots, std::vector<std::uint16_t>& species, bool& anyWrite, bool& anyFailure,
                                    const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads) {
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto& summary = session_.partyWorking.summaries[slot];
        const auto& baseline = session_.partyBaseline.summaries[slot];
        const bool sameSummary = summary.species == baseline.species && summary.nickname == baseline.nickname;
        const bool samePayload = session_.partyWorking.payloads[slot].data == session_.partyBaseline.payloads[slot].data;
        if (sameSummary && samePayload) {
            continue;
        }
        if (summary.species == 0) {
            const auto& vacatedPayload = session_.partyBaseline.payloads[slot].data;
            const bool stillPendingElsewhere = !vacatedPayload.empty() && std::any_of(
                unresolvedPayloads.begin(), unresolvedPayloads.end(),
                [&](const std::vector<std::uint8_t>& payload) { return payload == vacatedPayload; });
            if (stillPendingElsewhere) {
                Logger::instance().warning("Commit: keeping party slot " + std::to_string(slot + 1)
                                           + " in place - its cloud upload was rejected");
                result_.skipped.push_back(CommitSkippedItem{
                    baseline.nickname.empty() ? "Pokemon #" + std::to_string(baseline.species) : baseline.nickname,
                    "Party slot " + std::to_string(slot + 1),
                    "Its move to the cloud was rejected, so it was kept here."
                });
                anyFailure = true;
                continue;
            }
            if (!session_.saveAdapter.clearPartySlot(slot)) {
                Logger::instance().warning("Commit: could not clear party slot " + std::to_string(slot + 1)
                                           + ", leaving it as-is");
                anyFailure = true;
                continue;
            }
        } else if (!session_.partyWorking.payloads[slot].data.empty()) {
            const auto& payload = session_.partyWorking.payloads[slot];
            if (!session_.saveAdapter.writePartyPokemon(slot, payload.format, payload.data)) {
                result_.skipped.push_back(CommitSkippedItem{
                    summary.nickname.empty() ? "Pokemon #" + std::to_string(summary.species) : summary.nickname,
                    "Party slot " + std::to_string(slot + 1),
                    "Incompatible generation for this save."
                });
                anyFailure = true;
                continue;
            }
            slots.push_back(slot);
            species.push_back(summary.species);
            ++result_.downloads;
        }
        anyWrite = true;
        advanceProgress();
    }
}

bool CommitService::writeSaveAndVerify(const std::vector<LocalWriteVerification>& localVerifications,
                                        const std::vector<std::size_t>& partySlots,
                                        const std::vector<std::uint16_t>& partySpecies) {
    phase_.store(3, std::memory_order_release);
    std::string saveError;
    if (!session_.saveAdapter.writeSave(saveError)) {
        Logger::instance().error("Commit: save write failed: " + saveError);
        return false;
    }
    advanceProgress();

    for (const auto& expected : localVerifications) {
        const auto savedBox = session_.saveAdapter.readBox(expected.box);
        if (expected.slot >= savedBox.size() || savedBox[expected.slot].species != expected.species) {
            Logger::instance().error("Post-save verification failed for box "
                                     + std::to_string(expected.box + 1) + " slot "
                                     + std::to_string(expected.slot + 1));
            return false;
        }
    }

    if (!partySlots.empty()) {
        const auto savedParty = session_.saveAdapter.readParty();
        for (std::size_t i = 0; i < partySlots.size(); ++i) {
            const std::size_t slot = partySlots[i];
            if (slot >= savedParty.size() || savedParty[slot].species != partySpecies[i]) {
                Logger::instance().error("Post-save party verification failed for slot "
                                         + std::to_string(slot + 1));
                return false;
            }
        }
    }
    return true;
}

void CommitService::revertCloudSlots(const std::vector<BankSlot>& slots) {
    for (const auto& [boxPosition, slot] : slots) {
        if (boxPosition == 0 || slot == 0 || slot > 30) {
            continue;
        }
        auto it = session_.cloudBoxes.find(static_cast<std::uint16_t>(boxPosition - 1));
        if (it == session_.cloudBoxes.end()) {
            continue;
        }
        // Not actually deleted - put the draft back to what's really there.
        it->second.summaries[slot - 1] = it->second.baseline[slot - 1];
    }
}

void CommitService::runDeletes(const std::vector<BankSlot>& deletes) {
    if (deletes.empty()) {
        return;
    }
    phase_.store(1, std::memory_order_release);
    for (const auto& deletion : deletes) {
        DeleteResult dr = app_.api_.deleteCloudPokemon(deletion.first, deletion.second, app_.session_.accessToken);
        if (!dr.success) {
            Logger::instance().warning("Commit: cloud delete failed for Bank " + std::to_string(deletion.first)
                                       + " Slot " + std::to_string(deletion.second) + ": " + dr.message);
            revertCloudSlots({deletion});
            continue;
        }
        auto it = session_.cloudBoxes.find(static_cast<std::uint16_t>(deletion.first - 1));
        if (it != session_.cloudBoxes.end()) {
            it->second.baseline[deletion.second - 1] = PokemonSummary{};
        }
        ++result_.deletes;
        advanceProgress();
    }
}

void CommitService::runCommit() {
    result_ = CommitResult{};

    std::vector<UploadPokemon> uploads;
    std::vector<BankSlot> deletes;
    collectCloudChanges(uploads, deletes);

    const std::size_t localChangeCount = countLocalChanges();
    const std::size_t partyChangeCount = countPartyChanges();
    const std::size_t uploadBatchCount = (uploads.size() + 29) / 30;
    totalSteps_ = localChangeCount + partyChangeCount
        + (localChangeCount + partyChangeCount > 0 ? 1 : 0)
        + deletes.size() + uploadBatchCount;
    completedSteps_ = 0;

    const std::vector<UploadPokemon> attempted = uploads;
    const std::vector<UploadPokemon> uploaded = runUploads(std::move(uploads));
    commitCloudUploadBaseline(uploaded);

    // Payload bytes of every Pokemon that was supposed to move to the cloud
    // this round but whose upload didn't go through - their local/party
    // source slot must stay put rather than being cleared, or they'd vanish
    // entirely (rejected on the server, then deleted locally too).
    std::vector<std::vector<std::uint8_t>> unresolvedPayloads;
    for (const auto& attempt : attempted) {
        const bool succeeded = std::any_of(uploaded.begin(), uploaded.end(), [&](const UploadPokemon& u) {
            return u.boxPosition == attempt.boxPosition && u.slot == attempt.slot;
        });
        if (!succeeded) {
            unresolvedPayloads.push_back(attempt.payload);
        }
    }

    bool anyLocalWrite = false;
    bool anyLocalFailure = false;
    std::vector<LocalWriteVerification> localVerifications;
    runLocalWrites(localVerifications, anyLocalWrite, anyLocalFailure, unresolvedPayloads);

    std::vector<std::size_t> partySlots;
    std::vector<std::uint16_t> partySpecies;
    runPartyWrites(partySlots, partySpecies, anyLocalWrite, anyLocalFailure, unresolvedPayloads);

    if (anyLocalWrite && !writeSaveAndVerify(localVerifications, partySlots, partySpecies)) {
        // The whole local flush failed (not a single-Pokemon problem) - none
        // of this round's local/party writes actually reached the save file,
        // so none of the cloud-side deletes tied to "moved to local" are
        // safe either. Nothing here has silently duplicated: the uploads
        // that already succeeded stayed committed above, and every local
        // slot still holds whatever is really in the (unflushed) save.
        result_.message = "Local save write failed. Uploads that already completed were kept; nothing was deleted from the cloud.";
        revertCloudSlots(deletes);
        result_.success = true;
        return;
    }

    // A delete removes the cloud original of something that just moved to
    // local storage. Only run these once we know every local/party write in
    // this round actually succeeded - otherwise we can't tell which deletes
    // are still backed by a real local copy, so we skip all of them this
    // round rather than risk erasing one whose local write failed.
    if (!anyLocalFailure) {
        runDeletes(deletes);
    } else {
        revertCloudSlots(deletes);
    }

    result_.success = true;
    progress_.store(100, std::memory_order_release);
    std::string message = "Uploaded " + std::to_string(result_.uploads)
                         + ", removed " + std::to_string(result_.deletes)
                         + ", saved locally.";
    if (!result_.skipped.empty()) {
        message += " " + std::to_string(result_.skipped.size()) + " Pokemon could not be moved and stayed put.";
    }
    result_.message = message;
}

void CommitService::poll() {
    if (!job_.poll()) {
        return;
    }
    app_.status_ = result_.message;
    const StoragePane previousPane = session_.storagePane;
    const std::size_t previousSlot = session_.focusedSlot;
    session_.localBaselines.clear();
    session_.localDrafts.clear();
    for (auto& [key, draft] : session_.cloudBoxes) {
        draft.summaries = draft.baseline;
        draft.pending = {};
    }
    storage_.loadLocalBox();
    storage_.refreshCloudBox(true);
    session_.storagePane = previousPane;
    session_.focusedSlot = previousSlot;
    session_.partyBaseline.summaries = session_.saveAdapter.readParty();
    for (std::size_t slot = 0; slot < 6; ++slot) {
        session_.partyBaseline.payloads[slot] = session_.saveAdapter.readPartyPokemon(slot);
    }
    session_.partyWorking = session_.partyBaseline;

    if (!result_.skipped.empty()) {
        Logger::instance().warning("Commit: " + std::to_string(result_.skipped.size()) + " Pokemon skipped");
        session_.errorDialogTitle = "SOME TRANSFERS SKIPPED";
        session_.errorDialogPokemon = result_.problemPokemon.empty() ? "Some Pokemon" : result_.problemPokemon;
        session_.errorDialogLocation = result_.problemLocation;
        session_.errorDialogMessage = result_.problemReason.empty()
            ? "Could not be moved and stayed where they were."
            : result_.problemReason;
        session_.errorDialogVisible = true;
    }
}
