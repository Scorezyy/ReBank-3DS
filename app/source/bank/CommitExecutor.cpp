#include "bank/CommitExecutor.hpp"

#include "core/PayloadHash.hpp"

#include <algorithm>
#include <map>
#include <utility>

namespace {
const char* kindName(TransferKind kind) {
    switch (kind) {
        case TransferKind::LocalMove:
            return "local-move";
        case TransferKind::Upload:
            return "upload";
        case TransferKind::CloudMove:
            return "cloud-move";
        case TransferKind::Download:
            return "download";
        case TransferKind::LocalDelete:
            return "local-delete";
        case TransferKind::CloudDelete:
            return "cloud-delete";
    }
    return "unknown";
}

std::size_t countKind(const CommitPlan& plan, TransferKind kind) {
    return static_cast<std::size_t>(std::count_if(plan.transfers.begin(), plan.transfers.end(),
        [kind](const PlannedTransfer& transfer) { return transfer.kind == kind; }));
}

std::size_t batchesFor(std::size_t count) {
    return (count + CommitExecutor::BatchSize - 1) / CommitExecutor::BatchSize;
}
}

CommitExecutor::CommitExecutor(const CommitPlan& plan, CommitBackend& backend, ProgressFn progress)
    : plan_(plan),
      backend_(backend),
      progress_(std::move(progress)),
      current_(plan.baselineLayout),
      uploads_(plan.transfers.size(), UploadState::NotAttempted),
      cancelledDownloads_(plan.transfers.size(), false) {}

CommitResult CommitExecutor::run() {
    for (const HeldBackPokemon& held : plan_.heldBack) {
        result_.issues.push_back({describePokemon(held.summary), held.location, held.reason, {}});
        backend_.log("commit: held back " + describeSlot(held.location) + " species "
                     + std::to_string(held.summary.species) + ": " + describeReason(held.reason));
    }
    for (const PlannedTransfer& transfer : plan_.transfers) {
        backend_.log(std::string("commit plan: ") + kindName(transfer.kind) + " " + describeSlot(transfer.origin)
                     + " -> " + describeSlot(transfer.destination)
                     + (transfer.staging ? " parked at " + describeSlot(*transfer.staging) : std::string{})
                     + " species " + std::to_string(transfer.summary.species)
                     + " payload=" + payloadTag(transfer.payload.data));
    }
    totalSteps_ = std::max<std::size_t>(1, plan_.cloudMoves.size() + 2
        + batchesFor(countKind(plan_, TransferKind::Upload))
        + batchesFor(countKind(plan_, TransferKind::Download) + countKind(plan_, TransferKind::CloudDelete)));

    if (plan_.empty()) {
        result_.success = result_.issues.empty();
        return result_;
    }
    if (!runCloudMoves()) {
        return result_;
    }
    claimCloudSources();
    if (!runFirstSave()) {
        return result_;
    }
    runUploads();
    runFinalSave();
    reportLocalPlacements();
    runCloudDeletes();
    countLocalChanges();
    progress_(CommitPhase::RemovingCloud, 100);
    result_.success = true;
    return result_;
}

bool CommitExecutor::active(std::size_t transfer) const {
    return droppedGroups_.count(plan_.transfers[transfer].group) == 0;
}

bool CommitExecutor::needsSecondSave() const {
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        if (active(index) && (transfer.kind == TransferKind::Upload || transfer.staging)) {
            return true;
        }
    }
    return false;
}

bool CommitExecutor::runCloudMoves() {
    for (const CloudMoveStep& step : plan_.cloudMoves) {
        advance(CommitPhase::MovingCloud);
        const RemoteResult moved = backend_.moveCloud(step.from, step.to);
        if (moved.outcome == RemoteOutcome::Done) {
            ++result_.moves;
            continue;
        }
        backend_.log("commit: cloud move " + describeSlot(step.from) + " -> " + describeSlot(step.to)
                     + " failed: " + moved.message);
        for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
            if (plan_.transfers[index].kind == TransferKind::CloudMove && plan_.transfers[index].destination == step.to) {
                addIssue(index, step.from, IssueReason::CloudMoveFailed, moved.message);
            }
        }
        result_.stop = CommitStop::CloudMoveFailed;
        result_.stopDetail = moved.message;
        return false;
    }
    return true;
}

CommitExecutor::LayoutOutcome CommitExecutor::applyLayout(const LocalLayout& target, bool finalWrite) {
    for (const auto& [slot, occupant] : target) {
        const auto existing = current_.find(slot);
        const std::optional<std::size_t> before = existing == current_.end() ? std::nullopt : existing->second;
        if (before == occupant) {
            continue;
        }
        const bool ok = occupant ? backend_.writeLocal(slot, plan_.transfers[*occupant].payload)
                                 : backend_.clearLocal(slot);
        if (!ok) {
            return {false, occupant ? occupant : before, occupant.has_value()};
        }
    }
    if (!backend_.persistLocal(saveError_, finalWrite)) {
        backend_.log("commit: save write failed: " + saveError_);
        return {false, std::nullopt, false};
    }
    for (const auto& [slot, occupant] : target) {
        current_[slot] = occupant;
    }
    return {true, std::nullopt, false};
}

void CommitExecutor::claimCloudSources() {
    std::vector<std::size_t> sources;
    std::vector<ClaimItem> items;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        const bool leavesCloud = transfer.kind == TransferKind::Download || transfer.kind == TransferKind::CloudDelete;
        if (!leavesCloud || !active(index) || !transfer.cloudSlotAfterMoves) {
            continue;
        }
        sources.push_back(index);
        items.push_back({*transfer.cloudSlotAfterMoves, transfer.payload});
    }
    for (std::size_t start = 0; start < sources.size(); start += BatchSize) {
        const std::size_t end = std::min(sources.size(), start + BatchSize);
        const ClaimBatchResult claimed = backend_.claimCloud(
            std::vector<ClaimItem>(items.begin() + static_cast<std::ptrdiff_t>(start),
                                   items.begin() + static_cast<std::ptrdiff_t>(end)));
        for (std::size_t offset = 0; start + offset < end; ++offset) {
            const std::size_t index = sources[start + offset];
            if (!active(index)) {
                continue;
            }
            if (claimed.outcome != RemoteOutcome::Done || offset >= claimed.claims.size()) {
                dropGroup(index, IssueReason::CloudClaimFailed);
                continue;
            }
            if (claimed.claims[offset] == CloudClaim::Locked) {
                dropGroup(index, IssueReason::CloudSlotTrading);
            } else if (claimed.claims[offset] == CloudClaim::Changed) {
                dropGroup(index, IssueReason::CloudSlotDifferent);
            }
        }
        if (claimed.outcome != RemoteOutcome::Done) {
            backend_.log("commit: cloud claim failed: " + claimed.message);
        }
    }
}

LocalLayout CommitExecutor::firstSaveTarget() const {
    LocalLayout target = plan_.stagedLayout;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        if (active(index)) {
            continue;
        }
        const PlannedTransfer& transfer = plan_.transfers[index];
        for (const std::optional<SlotRef>& slot : {std::optional<SlotRef>(transfer.origin),
                                                   std::optional<SlotRef>(transfer.destination), transfer.staging}) {
            if (!slot || slot->isCloud()) {
                continue;
            }
            const auto baseline = plan_.baselineLayout.find(*slot);
            target[*slot] = baseline == plan_.baselineLayout.end() ? std::nullopt : baseline->second;
        }
    }
    return target;
}

void CommitExecutor::dropGroup(std::size_t transfer, IssueReason reason) {
    const std::size_t group = plan_.transfers[transfer].group;
    droppedGroups_.insert(group);
    addIssue(transfer, plan_.transfers[transfer].origin, reason);
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        if (index != transfer && plan_.transfers[index].group == group) {
            addIssue(index, plan_.transfers[index].origin, IssueReason::LinkedToHeldBack);
        }
    }
}

bool CommitExecutor::runFirstSave() {
    advance(CommitPhase::SavingLocal);
    while (true) {
        const LayoutOutcome outcome = applyLayout(firstSaveTarget(), !needsSecondSave());
        if (outcome.ok) {
            return true;
        }
        backend_.discardLocal();
        if (!outcome.failedTransfer) {
            result_.stop = CommitStop::SaveFailed;
            result_.stopDetail = saveError_;
            return false;
        }
        dropGroup(*outcome.failedTransfer, outcome.failedWrite ? IssueReason::WriteFailed : IssueReason::RemoveFailed);
    }
}

void CommitExecutor::runUploads() {
    std::vector<std::size_t> pending;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        if (plan_.transfers[index].kind == TransferKind::Upload && active(index)) {
            pending.push_back(index);
        }
    }
    for (std::size_t start = 0; start < pending.size(); start += BatchSize) {
        advance(CommitPhase::Uploading);
        const std::size_t end = std::min(pending.size(), start + BatchSize);
        uploadBatch(std::vector<std::size_t>(pending.begin() + static_cast<std::ptrdiff_t>(start),
                                             pending.begin() + static_cast<std::ptrdiff_t>(end)));
    }
    reconcileUnknownUploads();
}

void CommitExecutor::uploadBatch(std::vector<std::size_t> batch) {
    while (!batch.empty()) {
        std::vector<UploadItem> items;
        items.reserve(batch.size());
        for (const std::size_t index : batch) {
            const PlannedTransfer& transfer = plan_.transfers[index];
            items.push_back({transfer.destination, transfer.summary, transfer.payload, transfer.displaced.has_value()});
        }
        const UploadBatchResult sent = backend_.uploadBatch(items);
        if (sent.outcome == RemoteOutcome::Done) {
            for (const std::size_t index : batch) {
                uploads_[index] = UploadState::Stored;
                ++result_.uploads;
            }
            return;
        }
        if (sent.outcome == RemoteOutcome::Unknown) {
            backend_.log("commit: upload outcome unknown (" + sent.message + "), checking the cloud");
            for (const std::size_t index : batch) {
                uploads_[index] = UploadState::Unknown;
            }
            return;
        }
        std::vector<std::size_t> remaining;
        for (const std::size_t index : batch) {
            const PlannedTransfer& transfer = plan_.transfers[index];
            const auto rejection = std::find_if(sent.rejected.begin(), sent.rejected.end(),
                [&](const CloudRejection& candidate) { return candidate.slot == transfer.destination; });
            if (rejection == sent.rejected.end()) {
                remaining.push_back(index);
                continue;
            }
            uploads_[index] = UploadState::Refused;
            addIssue(index, transfer.origin, IssueReason::CloudRefused, rejection->reason);
        }
        if (remaining.size() == batch.size()) {
            for (const std::size_t index : batch) {
                uploads_[index] = UploadState::Refused;
                addIssue(index, plan_.transfers[index].origin, IssueReason::CloudRefused, sent.message);
            }
            return;
        }
        batch = std::move(remaining);
    }
}

void CommitExecutor::reconcileUnknownUploads() {
    std::map<std::uint16_t, std::vector<std::size_t>> byBox;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        if (uploads_[index] == UploadState::Unknown) {
            byBox[plan_.transfers[index].destination.box].push_back(index);
        }
    }
    for (const auto& [box, transfers] : byBox) {
        const auto listing = backend_.readCloudBox(box);
        for (const std::size_t index : transfers) {
            const PlannedTransfer& transfer = plan_.transfers[index];
            if (!listing) {
                addIssue(index, transfer.origin, IssueReason::UploadUnconfirmed, describeSlot(transfer.destination));
                continue;
            }
            const PokemonPayload& found = (*listing)[transfer.destination.slot];
            const bool displacedStillThere = transfer.displaced
                && found == plan_.transfers[*transfer.displaced].payload;
            if (found == transfer.payload) {
                uploads_[index] = UploadState::Stored;
                ++result_.uploads;
                backend_.log("commit: upload to " + describeSlot(transfer.destination) + " confirmed by listing");
            } else if (!found.known() || displacedStillThere) {
                uploads_[index] = UploadState::Refused;
                addIssue(index, transfer.origin, IssueReason::UploadNotStored);
            } else {
                addIssue(index, transfer.origin, IssueReason::CloudSlotDifferent);
            }
        }
    }
}

CommitExecutor::CloudCopy CommitExecutor::cloudCopyOf(std::size_t transfer) const {
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& upload = plan_.transfers[index];
        if (upload.kind != TransferKind::Upload || upload.displaced != transfer) {
            continue;
        }
        switch (uploads_[index]) {
            case UploadState::Stored:
            case UploadState::Undone:
                return CloudCopy::Overwritten;
            case UploadState::Unknown:
                return CloudCopy::Unknown;
            default:
                return CloudCopy::Intact;
        }
    }
    return CloudCopy::Intact;
}

std::optional<SlotRef> CommitExecutor::localSlotOf(std::size_t transfer) const {
    for (const auto& [slot, occupant] : current_) {
        if (occupant == transfer) {
            return slot;
        }
    }
    return std::nullopt;
}

void CommitExecutor::runFinalSave() {
    if (!needsSecondSave()) {
        return;
    }
    LocalLayout target = current_;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        if (transfer.kind == TransferKind::Upload && uploads_[index] == UploadState::Stored) {
            target[transfer.origin] = std::nullopt;
        }
    }
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        if (!transfer.staging || !active(index)) {
            continue;
        }
        if (!target[transfer.destination]) {
            target[transfer.destination] = index;
            target[*transfer.staging] = std::nullopt;
        } else if (transfer.kind == TransferKind::Download && cloudCopyOf(index) == CloudCopy::Intact) {
            target[*transfer.staging] = std::nullopt;
            cancelledDownloads_[index] = true;
        } else if (transfer.kind == TransferKind::LocalMove) {
            const auto origin = target.find(transfer.origin);
            if (origin != target.end() && !origin->second) {
                origin->second = index;
                target[*transfer.staging] = std::nullopt;
            }
        }
    }
    advance(CommitPhase::Finalizing);
    if (applyLayout(target, true).ok) {
        return;
    }
    backend_.discardLocal();
    std::fill(cancelledDownloads_.begin(), cancelledDownloads_.end(), false);
    compensateStoredUploads();
}

void CommitExecutor::compensateStoredUploads() {
    backend_.log("commit: final save failed (" + saveError_ + "), undoing confirmed uploads");
    std::vector<std::size_t> stored;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        if (plan_.transfers[index].kind == TransferKind::Upload && uploads_[index] == UploadState::Stored) {
            stored.push_back(index);
        }
    }
    for (std::size_t start = 0; start < stored.size(); start += BatchSize) {
        const std::size_t end = std::min(stored.size(), start + BatchSize);
        std::vector<SlotRef> slots;
        for (std::size_t position = start; position < end; ++position) {
            slots.push_back(plan_.transfers[stored[position]].destination);
        }
        const bool removed = backend_.deleteCloud(slots).outcome == RemoteOutcome::Done;
        for (std::size_t position = start; position < end; ++position) {
            const std::size_t index = stored[position];
            const PlannedTransfer& transfer = plan_.transfers[index];
            if (removed) {
                uploads_[index] = UploadState::Undone;
                --result_.uploads;
                addIssue(index, transfer.origin, IssueReason::UploadUndone);
            } else {
                addIssue(index, transfer.origin, IssueReason::UploadNotUndone, describeSlot(transfer.destination));
            }
        }
    }
}

void CommitExecutor::reportLocalPlacements() {
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        if (!transfer.landsLocally() || !active(index)) {
            continue;
        }
        const std::optional<SlotRef> placed = localSlotOf(index);
        if (cancelledDownloads_[index]) {
            addIssue(index, transfer.origin, IssueReason::SwapCancelled);
        } else if (placed && transfer.staging && *placed == *transfer.staging) {
            addIssue(index, *placed, IssueReason::PlacedElsewhere);
        } else if (placed && *placed == transfer.origin) {
            addIssue(index, *placed, IssueReason::ReturnedToOrigin);
        }
    }
}

void CommitExecutor::runCloudDeletes() {
    std::vector<std::pair<std::size_t, SlotRef>> targets;
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        const bool download = transfer.kind == TransferKind::Download;
        if ((!download && transfer.kind != TransferKind::CloudDelete) || !active(index)) {
            continue;
        }
        if (download && (cancelledDownloads_[index] || !localSlotOf(index))) {
            continue;
        }
        switch (cloudCopyOf(index)) {
            case CloudCopy::Overwritten:
                ++(download ? result_.downloads : result_.deletes);
                break;
            case CloudCopy::Unknown:
                if (download) {
                    ++result_.downloads;
                }
                addIssue(index, *transfer.cloudSlotAfterMoves, IssueReason::OverwriteUnconfirmed);
                break;
            case CloudCopy::Intact:
                targets.emplace_back(index, *transfer.cloudSlotAfterMoves);
                break;
        }
    }
    for (std::size_t start = 0; start < targets.size(); start += BatchSize) {
        const std::size_t end = std::min(targets.size(), start + BatchSize);
        deleteFromCloud({targets.begin() + static_cast<std::ptrdiff_t>(start),
                         targets.begin() + static_cast<std::ptrdiff_t>(end)});
    }
}

void CommitExecutor::deleteFromCloud(const std::vector<std::pair<std::size_t, SlotRef>>& targets) {
    advance(CommitPhase::RemovingCloud);
    std::vector<SlotRef> slots;
    slots.reserve(targets.size());
    for (const auto& target : targets) {
        slots.push_back(target.second);
    }
    const RemoteResult removed = backend_.deleteCloud(slots);
    if (removed.outcome != RemoteOutcome::Done) {
        backend_.log("commit: cloud delete of " + std::to_string(slots.size()) + " slot(s) failed: " + removed.message);
    }
    for (const auto& [index, slot] : targets) {
        const bool download = plan_.transfers[index].kind == TransferKind::Download;
        if (removed.outcome == RemoteOutcome::Done) {
            ++(download ? result_.downloads : result_.deletes);
        } else {
            addIssue(index, slot, download ? IssueReason::DownloadNotRemoved : IssueReason::CloudRemoveFailed,
                     removed.message);
        }
    }
}

void CommitExecutor::countLocalChanges() {
    for (std::size_t index = 0; index < plan_.transfers.size(); ++index) {
        const PlannedTransfer& transfer = plan_.transfers[index];
        if (!active(index)) {
            continue;
        }
        if (transfer.kind == TransferKind::LocalMove && localSlotOf(index) == transfer.destination) {
            ++result_.moves;
        } else if (transfer.kind == TransferKind::LocalDelete && !localSlotOf(index)) {
            ++result_.deletes;
        }
    }
}

void CommitExecutor::addIssue(std::size_t transfer, const SlotRef& location, IssueReason reason, std::string detail) {
    const PokemonSummary& summary = plan_.transfers[transfer].summary;
    backend_.log("commit issue: " + describeSlot(location) + " species " + std::to_string(summary.species)
                 + ": " + describeReason(reason) + (detail.empty() ? std::string{} : " (" + detail + ")"));
    result_.issues.push_back({describePokemon(summary), location, reason, std::move(detail)});
}

void CommitExecutor::advance(CommitPhase phase) {
    completedSteps_ = std::min(completedSteps_ + 1, totalSteps_);
    progress_(phase, static_cast<int>(completedSteps_ * 100 / totalSteps_));
}
