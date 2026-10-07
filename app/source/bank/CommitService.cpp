#include "bank/CommitService.hpp"

#include "bank/DeviceCommitBackend.hpp"
#include "bank/LoadService.hpp"
#include "core/Logger.hpp"

#include <utility>

namespace {
TextId issueText(IssueReason reason) {
    switch (reason) {
        case IssueReason::DataNotLoaded: return TextId::IssueDataNotLoaded;
        case IssueReason::OriginUnconfirmed: return TextId::IssueOriginUnconfirmed;
        case IssueReason::NotInTrash: return TextId::IssueNotInTrash;
        case IssueReason::LinkedToHeldBack: return TextId::IssueLinkedToHeldBack;
        case IssueReason::NoParkingSlot: return TextId::IssueNoParkingSlot;
        case IssueReason::CloudMoveFailed: return TextId::IssueCloudMoveFailed;
        case IssueReason::WriteFailed: return TextId::IssueWriteFailed;
        case IssueReason::RemoveFailed: return TextId::IssueRemoveFailed;
        case IssueReason::CloudRefused: return TextId::IssueCloudRefused;
        case IssueReason::UploadUnconfirmed: return TextId::IssueUploadUnconfirmed;
        case IssueReason::UploadNotStored: return TextId::IssueUploadNotStored;
        case IssueReason::CloudSlotDifferent: return TextId::IssueCloudSlotDifferent;
        case IssueReason::UploadUndone: return TextId::IssueUploadUndone;
        case IssueReason::UploadNotUndone: return TextId::IssueUploadNotUndone;
        case IssueReason::SwapCancelled: return TextId::IssueSwapCancelled;
        case IssueReason::PlacedElsewhere: return TextId::IssuePlacedElsewhere;
        case IssueReason::ReturnedToOrigin: return TextId::IssueReturnedToOrigin;
        case IssueReason::OverwriteUnconfirmed: return TextId::IssueOverwriteUnconfirmed;
        case IssueReason::DownloadNotRemoved: return TextId::IssueDownloadNotRemoved;
        case IssueReason::CloudRemoveFailed: return TextId::IssueCloudRemoveFailed;
        case IssueReason::CloudSlotTrading: return TextId::IssueCloudSlotTrading;
        case IssueReason::CloudClaimFailed: return TextId::IssueCloudClaimFailed;
    }
    return TextId::IssueLinkedToHeldBack;
}
}

void CommitService::begin() {
    if (running()) {
        return;
    }
    if (!context_.signedIn()) {
        context_.status = context_.text.get(TextId::SignInAgain);
        return;
    }
    if (context_.loads.busy()) {
        requestWhenIdle();
        return;
    }
    requested_ = false;
    snapshot_ = captureSnapshot();
    accessToken_ = context_.account.accessToken;
    phase_.store(static_cast<int>(CommitPhase::Preparing), std::memory_order_release);
    progress_.store(0, std::memory_order_release);
    context_.status = context_.text.get(TextId::CommitRunning);
    Logger::instance().info("commit: starting with " + std::to_string(snapshot_.localDrafts.size())
                            + " local box drafts, " + std::to_string(snapshot_.cloudBoxes.size())
                            + " loaded cloud boxes, " + std::to_string(snapshot_.confirmedDeletions.size())
                            + " confirmed deletions");
    if (!job_.start([this]() { runCommit(); })) {
        context_.status = context_.text.get(TextId::CommitStartFailed);
    }
}

void CommitService::requestWhenIdle() {
    requested_ = true;
    context_.status = context_.text.get(TextId::CommitRunning);
}

void CommitService::pumpRequest(bool canStart) {
    if (!requested_ || !canStart || context_.loads.busy()) {
        return;
    }
    requested_ = false;
    if (!storage_.hasPendingChanges()) {
        context_.status = context_.text.get(TextId::NothingToCommit);
        return;
    }
    begin();
}

CommitSnapshot CommitService::captureSnapshot() {
    storage_.persistDrafts();
    CommitSnapshot snapshot;
    snapshot.localDrafts = session_.localDrafts;
    snapshot.localBaselines = session_.localBaselines;
    for (const auto& [box, draft] : snapshot.localDrafts) {
        if (!snapshot.localBaselines.count(box)) {
            snapshot.localBaselines[box] = session_.saveAdapter.readBox(box);
        }
    }
    snapshot.partyBaseline = session_.partyBaseline;
    snapshot.partyWorking = session_.party;
    snapshot.cloudBoxes = session_.cloudBoxes;
    snapshot.confirmedDeletions = session_.confirmedDeletions;
    return snapshot;
}

void CommitService::runCommit() {
    SaveAdapter& save = session_.saveAdapter;
    const LocalSlotProbe probe{save.boxCount(),
                               [&save](std::size_t box, std::size_t slot) { return save.slotOccupied(box, slot); },
                               save.boxCapacity()};
    const CommitPlan plan = planCommit(snapshot_, probe);
    DeviceCommitBackend backend(context_.api, save, accessToken_);
    CommitExecutor executor(plan, backend, [this](CommitPhase phase, int percent) {
        phase_.store(static_cast<int>(phase), std::memory_order_release);
        progress_.store(percent, std::memory_order_release);
    });
    result_ = executor.run();
    Logger::instance().info("commit: finished success=" + std::to_string(result_.success)
                            + " uploads=" + std::to_string(result_.uploads)
                            + " downloads=" + std::to_string(result_.downloads)
                            + " moves=" + std::to_string(result_.moves)
                            + " deletes=" + std::to_string(result_.deletes)
                            + " issues=" + std::to_string(result_.issues.size()));
}

void CommitService::poll() {
    if (!job_.poll()) {
        return;
    }
    applyResult();
}

std::string CommitService::describeLocation(const SlotRef& location) const {
    const std::string slot = std::to_string(location.slot + 1);
    switch (location.kind) {
        case SlotKind::Party:
            return context_.text.format(TextId::LocationParty, {slot});
        case SlotKind::Cloud:
            return context_.text.format(TextId::LocationBank, {std::to_string(location.box + 1), slot});
        case SlotKind::Local:
            break;
    }
    return context_.text.format(TextId::LocationBox, {std::to_string(location.box + 1), slot});
}

std::string CommitService::describeIssue(const CommitIssue& issue) const {
    return context_.text.format(issueText(issue.reason), {issue.detail});
}

void CommitService::applyResult() {
    snapshot_ = CommitSnapshot{};
    const StoragePane previousPane = session_.storagePane;
    const std::size_t previousSlot = session_.focusedSlot;
    storage_.reloadAfterCommit(result_.success && result_.issues.empty());
    session_.storagePane = previousPane;
    session_.focusedSlot = previousSlot;
    context_.status = context_.text.format(TextId::CommitSummary, {std::to_string(result_.uploads),
        std::to_string(result_.downloads), std::to_string(result_.moves), std::to_string(result_.deletes)});

    if (!result_.issues.empty()) {
        const CommitIssue& first = result_.issues.front();
        const std::size_t count = result_.issues.size();
        std::string title = !result_.success ? std::string(context_.text.get(TextId::CommitStoppedTitle))
            : count == 1 ? std::string(context_.text.get(TextId::CommitAttentionOne))
            : context_.text.format(TextId::CommitAttentionMany, {std::to_string(count)});
        context_.errors.show(std::move(title), describeIssue(first), first.pokemon, describeLocation(first.location));
    } else if (!result_.success) {
        const TextId message = result_.stop == CommitStop::CloudMoveFailed ? TextId::CommitStopCloudMove
                                                                             : TextId::CommitStopSave;
        context_.errors.show(std::string(context_.text.get(TextId::CommitStoppedTitle)),
                             context_.text.format(message, {result_.stopDetail}),
                             std::string(context_.text.get(TextId::YourChanges)));
    }
}
