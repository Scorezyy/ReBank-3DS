#include "bank/CommitService.hpp"

#include "app/App.hpp"
#include "core/Logger.hpp"

#include <algorithm>

void CommitService::fail(std::string message) {
    result_.message = std::move(message);
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

bool CommitService::collectCloudChanges(std::vector<UploadPokemon>& uploads,
                                         std::vector<std::pair<std::uint16_t, std::uint8_t>>& deletes) {
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
                    fail("A staged Pokemon is missing its payload.");
                    return false;
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
    return true;
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

void CommitService::parseUploadFailure(const std::string& message, const std::vector<UploadPokemon>& batch) {
    std::uint16_t problemBank = 0;
    const std::size_t bankMarker = message.find("Bank ");
    if (bankMarker != std::string::npos) {
        std::size_t begin = bankMarker + 5;
        std::size_t end = begin;
        while (end < message.size() && message[end] >= '0' && message[end] <= '9') {
            problemBank = static_cast<std::uint16_t>(problemBank * 10 + (message[end] - '0'));
            ++end;
        }
    }

    const std::size_t marker = message.find("Slot ");
    if (marker == std::string::npos) {
        return;
    }
    const std::size_t begin = marker + 5;
    std::size_t end = begin;
    std::uint8_t problemSlot = 0;
    while (end < message.size() && message[end] >= '0' && message[end] <= '9') {
        problemSlot = static_cast<std::uint8_t>(problemSlot * 10 + (message[end] - '0'));
        ++end;
    }
    if (end == begin) {
        return;
    }

    const auto problem = std::find_if(batch.begin(), batch.end(),
        [problemBank, problemSlot](const UploadPokemon& upload) {
            return upload.slot == problemSlot
                && (problemBank == 0 || upload.boxPosition == problemBank);
        });
    if (problem != batch.end()) {
        result_.problemPokemon = problem->nickname.empty()
            ? "Pokemon #" + std::to_string(problem->species)
            : problem->nickname;
        result_.problemLocation = "Bank " + std::to_string(problem->boxPosition)
            + "  |  Slot " + std::to_string(problem->slot);
    }

    const std::size_t reason = message.find(':', end);
    if (reason != std::string::npos && reason + 1 < message.size()) {
        result_.problemReason = message.substr(reason + 1);
        while (!result_.problemReason.empty() && result_.problemReason.front() == ' ') {
            result_.problemReason.erase(result_.problemReason.begin());
        }
    }
}

bool CommitService::runUploads(std::vector<UploadPokemon> uploads) {
    if (uploads.empty()) {
        return true;
    }
    phase_.store(2, std::memory_order_release);
    while (!uploads.empty()) {
        const std::size_t batchSize = std::min<std::size_t>(uploads.size(), 30);
        std::vector<UploadPokemon> batch(uploads.begin(), uploads.begin() + batchSize);
        uploads.erase(uploads.begin(), uploads.begin() + batchSize);
        UploadResult ur = app_.api_.uploadPokemon(batch, app_.session_.accessToken);
        if (!ur.success) {
            fail("Upload failed: " + ur.message);
            result_.problemReason = ur.message;
            parseUploadFailure(ur.message, batch);
            return false;
        }
        result_.uploads += ur.storedCount;
        advanceProgress();
    }
    return true;
}

bool CommitService::runLocalWrites(std::vector<LocalWriteVerification>& verifications, bool& anyWrite) {
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
                if (!session_.saveAdapter.clearSlot(boxKey, slot)) {
                    fail("Could not clear local slot " + std::to_string(slot + 1) + ".");
                    return false;
                }
            } else if (!draft.payloads[slot].data.empty()) {
                if (!session_.saveAdapter.writePokemon(boxKey, slot, draft.payloads[slot].format, draft.payloads[slot].data)) {
                    fail("Local write failed for slot " + std::to_string(slot + 1) + " (incompatible generation).");
                    return false;
                }
                verifications.push_back({boxKey, slot, draft.summaries[slot].species});
                ++result_.downloads;
            }
            anyWrite = true;
            advanceProgress();
        }
    }
    return true;
}

bool CommitService::runPartyWrites(std::vector<std::size_t>& slots, std::vector<std::uint16_t>& species, bool& anyWrite) {
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto& summary = session_.partyWorking.summaries[slot];
        const auto& baseline = session_.partyBaseline.summaries[slot];
        const bool sameSummary = summary.species == baseline.species && summary.nickname == baseline.nickname;
        const bool samePayload = session_.partyWorking.payloads[slot].data == session_.partyBaseline.payloads[slot].data;
        if (sameSummary && samePayload) {
            continue;
        }
        if (summary.species == 0) {
            if (!session_.saveAdapter.clearPartySlot(slot)) {
                fail("Could not clear party slot " + std::to_string(slot + 1) + ".");
                return false;
            }
        } else if (!session_.partyWorking.payloads[slot].data.empty()) {
            const auto& payload = session_.partyWorking.payloads[slot];
            if (!session_.saveAdapter.writePartyPokemon(slot, payload.format, payload.data)) {
                fail("Party write failed for slot " + std::to_string(slot + 1) + " (incompatible generation).");
                return false;
            }
            slots.push_back(slot);
            species.push_back(summary.species);
            ++result_.downloads;
        }
        anyWrite = true;
        advanceProgress();
    }
    return true;
}

bool CommitService::writeSaveAndVerify(const std::vector<LocalWriteVerification>& localVerifications,
                                        const std::vector<std::size_t>& partySlots,
                                        const std::vector<std::uint16_t>& partySpecies) {
    phase_.store(3, std::memory_order_release);
    std::string saveError;
    if (!session_.saveAdapter.writeSave(saveError)) {
        fail("Save write failed: " + saveError);
        return false;
    }
    advanceProgress();

    for (const auto& expected : localVerifications) {
        const auto savedBox = session_.saveAdapter.readBox(expected.box);
        if (expected.slot >= savedBox.size() || savedBox[expected.slot].species != expected.species) {
            fail("Saved Pokemon verification failed; cloud copy retained.");
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
                fail("Saved Pokemon verification failed; cloud copy retained.");
                Logger::instance().error("Post-save party verification failed for slot "
                                         + std::to_string(slot + 1));
                return false;
            }
        }
    }
    return true;
}

bool CommitService::runDeletes(const std::vector<std::pair<std::uint16_t, std::uint8_t>>& deletes) {
    if (deletes.empty()) {
        return true;
    }
    phase_.store(1, std::memory_order_release);
    for (const auto& deletion : deletes) {
        DeleteResult dr = app_.api_.deleteCloudPokemon(deletion.first, deletion.second, app_.session_.accessToken);
        if (!dr.success) {
            fail("Delete failed: " + dr.message);
            return false;
        }
        ++result_.deletes;
        advanceProgress();
    }
    return true;
}

void CommitService::runCommit() {
    result_ = CommitResult{};

    std::vector<UploadPokemon> uploads;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> deletes;
    if (!collectCloudChanges(uploads, deletes)) {
        return;
    }

    const std::size_t localChangeCount = countLocalChanges();
    const std::size_t partyChangeCount = countPartyChanges();
    const std::size_t uploadBatchCount = (uploads.size() + 29) / 30;
    totalSteps_ = localChangeCount + partyChangeCount
        + (localChangeCount + partyChangeCount > 0 ? 1 : 0)
        + deletes.size() + uploadBatchCount;
    completedSteps_ = 0;

    if (!runUploads(std::move(uploads))) {
        return;
    }

    bool anyLocalWrite = false;
    std::vector<LocalWriteVerification> localVerifications;
    if (!runLocalWrites(localVerifications, anyLocalWrite)) {
        return;
    }

    std::vector<std::size_t> partySlots;
    std::vector<std::uint16_t> partySpecies;
    if (!runPartyWrites(partySlots, partySpecies, anyLocalWrite)) {
        return;
    }

    if (anyLocalWrite && !writeSaveAndVerify(localVerifications, partySlots, partySpecies)) {
        return;
    }

    if (!runDeletes(deletes)) {
        return;
    }

    result_.success = true;
    progress_.store(100, std::memory_order_release);
    result_.message = "Uploaded " + std::to_string(result_.uploads)
                     + ", removed " + std::to_string(result_.deletes)
                     + ", saved locally.";
}

void CommitService::poll() {
    if (!job_.poll()) {
        return;
    }
    if (result_.success) {
        app_.status_ = result_.message;
        const StoragePane previousPane = session_.storagePane;
        const std::size_t previousSlot = session_.focusedSlot;
        session_.localBaselines.clear();
        session_.localDrafts.clear();
        for (auto& [key, draft] : session_.cloudBoxes) {
            draft.baseline = draft.summaries;
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
        return;
    }
    Logger::instance().warning("Commit failed: " + result_.message);
    const std::string failure = result_.message;
    storage_.discardPendingChanges();
    app_.status_ = failure + " Changes reloaded.";
    session_.errorDialogTitle = "TRANSFER BLOCKED";
    session_.errorDialogPokemon = result_.problemPokemon.empty() ? "Transfer failed" : result_.problemPokemon;
    session_.errorDialogLocation = result_.problemLocation;
    session_.errorDialogMessage = result_.problemReason.empty() ? failure : result_.problemReason;
    session_.errorDialogVisible = true;
}
