#include "bank/CommitService.hpp"

#include "app/App.hpp"
#include "core/Logger.hpp"
#include "save/PayloadHash.hpp"

#include <algorithm>

namespace {
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
            if (!initHas && !nowHas) {
                continue;
            }
            Logger::instance().info("collectCloudChanges: bank " + std::to_string(boxPosition) + " slot "
                                    + std::to_string(slot + 1) + " baseline species "
                                    + std::to_string(draft.baseline[slot].species) + " draft species "
                                    + std::to_string(draft.summaries[slot].species) + " pending="
                                    + payloadTag(draft.pending[slot].data) + " same=" + (same ? "1" : "0"));
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
    revertCloudSlots({{upload.boxPosition, upload.slot}});
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
        for (const auto& item : batch) {
            Logger::instance().info("runUploads: sending bank " + std::to_string(item.boxPosition) + " slot "
                                    + std::to_string(item.slot) + " species " + std::to_string(item.species)
                                    + " \"" + item.nickname + "\" payload=" + payloadTag(item.payload));
        }
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
        it->second.baseline[slot] = it->second.summaries[slot];
        it->second.pending[slot] = {};
        it->second.payloads[slot] = {item.format, item.payload};
        Logger::instance().info("commitCloudUploadBaseline: bank " + std::to_string(item.boxPosition) + " slot "
                                + std::to_string(item.slot) + " species " + std::to_string(item.species)
                                + " locked in as baseline, payload=" + payloadTag(item.payload));
    }
}

namespace {
bool payloadStillPending(const std::vector<std::uint8_t>& payload,
                          const std::vector<std::vector<std::uint8_t>>& pending) {
    return !payload.empty() && std::any_of(pending.begin(), pending.end(),
        [&](const std::vector<std::uint8_t>& other) { return other == payload; });
}
}

bool CommitService::clearLocalSlot(std::size_t boxKey, std::size_t slot, const LocalBoxDraft& baseline,
                                    const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads) {
    if (payloadStillPending(baseline.payloads[slot].data, unresolvedPayloads)) {
        result_.skipped.push_back(CommitSkippedItem{
            baseline.summaries[slot].nickname.empty()
                ? "Pokemon #" + std::to_string(baseline.summaries[slot].species)
                : baseline.summaries[slot].nickname,
            "Local box " + std::to_string(boxKey + 1) + "  |  Slot " + std::to_string(slot + 1),
            "Its move to the cloud was rejected, so it was kept here."
        });
        return false;
    }
    if (!session_.saveAdapter.clearSlot(boxKey, slot)) {
        return false;
    }
    Logger::instance().info("clearLocalSlot: local box " + std::to_string(boxKey + 1) + " slot "
                            + std::to_string(slot + 1) + " cleared, was species "
                            + std::to_string(baseline.summaries[slot].species) + " payload="
                            + payloadTag(baseline.payloads[slot].data));
    return true;
}

bool CommitService::writeLocalSlot(std::size_t boxKey, std::size_t slot, const LocalBoxDraft& draft,
                                    const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads) {
    if (payloadStillPending(draft.payloads[slot].data, stillOnCloudPayloads)) {
        result_.skipped.push_back(CommitSkippedItem{
            draft.summaries[slot].nickname.empty()
                ? "Pokemon #" + std::to_string(draft.summaries[slot].species)
                : draft.summaries[slot].nickname,
            "Local box " + std::to_string(boxKey + 1) + "  |  Slot " + std::to_string(slot + 1),
            "Its move from the cloud didn't complete, so it stayed there instead."
        });
        return false;
    }
    if (!session_.saveAdapter.writePokemon(boxKey, slot, draft.payloads[slot].format, draft.payloads[slot].data)) {
        result_.skipped.push_back(CommitSkippedItem{
            draft.summaries[slot].nickname.empty()
                ? "Pokemon #" + std::to_string(draft.summaries[slot].species)
                : draft.summaries[slot].nickname,
            "Local box " + std::to_string(boxKey + 1) + "  |  Slot " + std::to_string(slot + 1),
            "Incompatible generation for this save."
        });
        return false;
    }
    Logger::instance().info("writeLocalSlot: local box " + std::to_string(boxKey + 1) + " slot "
                            + std::to_string(slot + 1) + " written species "
                            + std::to_string(draft.summaries[slot].species) + " payload="
                            + payloadTag(draft.payloads[slot].data));
    ++result_.downloads;
    return true;
}

void CommitService::runLocalWrites(bool& anyWrite, bool& anyFailure,
                                    const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads,
                                    const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads) {
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
            const bool isClear = draft.summaries[slot].species == 0;
            const bool hasPayload = !draft.payloads[slot].data.empty();
            const bool ok = isClear
                ? clearLocalSlot(boxKey, slot, baselineIt->second, unresolvedPayloads)
                : hasPayload ? writeLocalSlot(boxKey, slot, draft, stillOnCloudPayloads) : true;
            if (!ok) {
                anyFailure = true;
                continue;
            }
            anyWrite = true;
            advanceProgress();
        }
    }
}

bool CommitService::clearPartySlotChecked(std::size_t slot,
                                           const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads) {
    const auto& baseline = session_.partyBaseline.summaries[slot];
    if (payloadStillPending(session_.partyBaseline.payloads[slot].data, unresolvedPayloads)) {
        result_.skipped.push_back(CommitSkippedItem{
            baseline.nickname.empty() ? "Pokemon #" + std::to_string(baseline.species) : baseline.nickname,
            "Party slot " + std::to_string(slot + 1),
            "Its move to the cloud was rejected, so it was kept here."
        });
        return false;
    }
    if (!session_.saveAdapter.clearPartySlot(slot)) {
        return false;
    }
    Logger::instance().info("clearPartySlotChecked: party slot " + std::to_string(slot + 1)
                            + " cleared, was species " + std::to_string(baseline.species) + " payload="
                            + payloadTag(session_.partyBaseline.payloads[slot].data));
    return true;
}

bool CommitService::writePartySlot(std::size_t slot, const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads) {
    const auto& summary = session_.partyWorking.summaries[slot];
    const auto& payload = session_.partyWorking.payloads[slot];
    if (payloadStillPending(payload.data, stillOnCloudPayloads)) {
        result_.skipped.push_back(CommitSkippedItem{
            summary.nickname.empty() ? "Pokemon #" + std::to_string(summary.species) : summary.nickname,
            "Party slot " + std::to_string(slot + 1),
            "Its move from the cloud didn't complete, so it stayed there instead."
        });
        return false;
    }
    if (!session_.saveAdapter.writePartyPokemon(slot, payload.format, payload.data)) {
        result_.skipped.push_back(CommitSkippedItem{
            summary.nickname.empty() ? "Pokemon #" + std::to_string(summary.species) : summary.nickname,
            "Party slot " + std::to_string(slot + 1),
            "Incompatible generation for this save."
        });
        return false;
    }
    Logger::instance().info("writePartySlot: party slot " + std::to_string(slot + 1) + " written species "
                            + std::to_string(summary.species) + " payload=" + payloadTag(payload.data));
    ++result_.downloads;
    return true;
}

void CommitService::runPartyWrites(bool& anyWrite, bool& anyFailure,
                                    const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads,
                                    const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads) {
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto& summary = session_.partyWorking.summaries[slot];
        const auto& baseline = session_.partyBaseline.summaries[slot];
        const bool sameSummary = summary.species == baseline.species && summary.nickname == baseline.nickname;
        const bool samePayload = session_.partyWorking.payloads[slot].data == session_.partyBaseline.payloads[slot].data;
        if (sameSummary && samePayload) {
            continue;
        }
        const bool isClear = summary.species == 0;
        const bool hasPayload = !session_.partyWorking.payloads[slot].data.empty();
        const bool ok = isClear
            ? clearPartySlotChecked(slot, unresolvedPayloads)
            : hasPayload ? writePartySlot(slot, stillOnCloudPayloads) : true;
        if (!ok) {
            anyFailure = true;
            continue;
        }
        anyWrite = true;
        advanceProgress();
    }
}

bool CommitService::writeSaveAndVerify() {
    phase_.store(3, std::memory_order_release);
    std::string saveError;
    if (!session_.saveAdapter.writeSave(saveError)) {
        Logger::instance().error("Commit: save write failed: " + saveError);
        return false;
    }
    advanceProgress();
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
        it->second.summaries[slot - 1] = it->second.baseline[slot - 1];
        it->second.pending[slot - 1] = {};
        Logger::instance().info("revertCloudSlots: bank " + std::to_string(boxPosition) + " slot "
                                + std::to_string(slot) + " reverted to baseline species "
                                + std::to_string(it->second.baseline[slot - 1].species));
    }
}

void CommitService::runDeletes(const std::vector<BankSlot>& deletes) {
    if (deletes.empty()) {
        return;
    }
    phase_.store(1, std::memory_order_release);
    for (const auto& deletion : deletes) {
        Logger::instance().info("runDeletes: deleting bank " + std::to_string(deletion.first) + " slot "
                                + std::to_string(deletion.second));
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
    Logger::instance().info("runCommit: starting, localDrafts=" + std::to_string(session_.localDrafts.size())
                            + " cloudBoxes=" + std::to_string(session_.cloudBoxes.size())
                            + " focusedSlot=" + std::to_string(session_.focusedSlot + 1)
                            + " pane=" + std::to_string(static_cast<int>(session_.storagePane)));

    std::vector<UploadPokemon> uploads;
    std::vector<BankSlot> deletes;
    collectCloudChanges(uploads, deletes);
    Logger::instance().info("runCommit: collected " + std::to_string(uploads.size()) + " uploads, "
                            + std::to_string(deletes.size()) + " deletes");

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
    std::vector<std::vector<std::uint8_t>> unresolvedPayloads;
    std::vector<std::vector<std::uint8_t>> stillOnCloudPayloads;
    for (const auto& attempt : attempted) {
        const bool succeeded = std::any_of(uploaded.begin(), uploaded.end(), [&](const UploadPokemon& u) {
            return u.boxPosition == attempt.boxPosition && u.slot == attempt.slot;
        });
        if (succeeded) {
            continue;
        }
        unresolvedPayloads.push_back(attempt.payload);
        if (attempt.boxPosition == 0 || attempt.slot == 0 || attempt.slot > 30) {
            continue;
        }
        const auto boxKey = static_cast<std::uint16_t>(attempt.boxPosition - 1);
        const auto boxIt = session_.cloudBoxes.find(boxKey);
        if (boxIt == session_.cloudBoxes.end()) {
            continue;
        }
        const auto& originalPayload = boxIt->second.payloads[attempt.slot - 1].data;
        if (!originalPayload.empty()) {
            stillOnCloudPayloads.push_back(originalPayload);
        }
    }
    if (!unresolvedPayloads.empty()) {
        for (const auto& [boxPosition, slot] : deletes) {
            if (boxPosition == 0 || slot == 0 || slot > 30) {
                continue;
            }
            const auto boxIt = session_.cloudBoxes.find(static_cast<std::uint16_t>(boxPosition - 1));
            if (boxIt == session_.cloudBoxes.end()) {
                continue;
            }
            const auto& sourcePayload = boxIt->second.payloads[slot - 1].data;
            if (!sourcePayload.empty()) {
                stillOnCloudPayloads.push_back(sourcePayload);
            }
        }
        Logger::instance().warning("Commit: upload rejected; cloud-source Pokemon stay out of the local save");
    }

    bool anyLocalWrite = false;
    bool anyLocalFailure = false;
    const bool deferLocalWrites = !unresolvedPayloads.empty() && !deletes.empty();
    if (deferLocalWrites) {
        anyLocalFailure = true;
        Logger::instance().warning("Commit: upload rejected while cloud Pokemon were moved locally; local draft deferred");
    } else {
        runLocalWrites(anyLocalWrite, anyLocalFailure, unresolvedPayloads, stillOnCloudPayloads);
        runPartyWrites(anyLocalWrite, anyLocalFailure, unresolvedPayloads, stillOnCloudPayloads);
    }

    if (anyLocalWrite && !writeSaveAndVerify()) {
        result_.message = "Local save write failed. Uploads that already completed were kept; nothing was deleted from the cloud.";
        revertCloudSlots(deletes);
        result_.success = true;
        return;
    }

    if (!anyLocalFailure) {
        runDeletes(deletes);
    } else {
        std::vector<BankSlot> cloudRelocationDeletes;
        std::vector<BankSlot> blockedDeletes;
        for (const auto& deletion : deletes) {
            if (deletion.first == 0 || deletion.second == 0 || deletion.second > 30) {
                blockedDeletes.push_back(deletion);
                continue;
            }
            const auto boxIt = session_.cloudBoxes.find(static_cast<std::uint16_t>(deletion.first - 1));
            if (boxIt == session_.cloudBoxes.end()) {
                blockedDeletes.push_back(deletion);
                continue;
            }
            const auto& sourcePayload = boxIt->second.payloads[deletion.second - 1].data;
            const bool copiedToCloud = !sourcePayload.empty() && std::any_of(
                uploaded.begin(), uploaded.end(), [&](const UploadPokemon& item) {
                    return item.payload == sourcePayload;
                });
            (copiedToCloud ? cloudRelocationDeletes : blockedDeletes).push_back(deletion);
        }
        runDeletes(cloudRelocationDeletes);
        revertCloudSlots(blockedDeletes);
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
    Logger::instance().info("runCommit: finished, uploads=" + std::to_string(result_.uploads)
                            + " deletes=" + std::to_string(result_.deletes)
                            + " downloads=" + std::to_string(result_.downloads)
                            + " skipped=" + std::to_string(result_.skipped.size()));
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
