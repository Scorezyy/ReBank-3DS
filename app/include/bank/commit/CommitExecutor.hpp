#pragma once

#include "bank/commit/CommitBackend.hpp"
#include "bank/commit/CommitPlan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

enum class CommitPhase : int {
    Preparing,
    MovingCloud,
    SavingLocal,
    Uploading,
    Finalizing,
    RemovingCloud
};

class CommitExecutor {
public:
    static constexpr std::size_t BatchSize = 30;

    using ProgressFn = std::function<void(CommitPhase, int)>;

    CommitExecutor(const CommitPlan& plan, CommitBackend& backend, ProgressFn progress);

    CommitResult run();

private:
    enum class UploadState : std::uint8_t { NotAttempted, Stored, Refused, Unknown, Undone };
    enum class CloudCopy : std::uint8_t { Intact, Overwritten, Unknown };

    struct LayoutOutcome {
        bool ok = false;
        std::optional<std::size_t> failedTransfer;
        bool failedWrite = false;
    };

    bool runCloudMoves();
    void claimCloudSources();
    void claimBatch(const std::vector<std::size_t>& batch);
    LayoutOutcome applyLayout(const LocalLayout& target, bool finalWrite);
    LocalLayout firstSaveTarget() const;
    bool runFirstSave();
    void dropGroup(std::size_t transfer, IssueReason reason);
    void runUploads();
    void uploadBatch(std::vector<std::size_t> batch);
    void reconcileUnknownUploads();
    void runFinalSave();
    void compensateStoredUploads();
    void reportLocalPlacements();
    void runCloudDeletes();
    void deleteFromCloud(const std::vector<std::pair<std::size_t, SlotRef>>& targets);
    void countLocalChanges();

    template <typename Predicate>
    std::vector<std::size_t> transfersWhere(Predicate predicate) const;
    bool active(std::size_t transfer) const;
    bool needsSecondSave() const;
    CloudCopy cloudCopyOf(std::size_t transfer) const;
    std::optional<SlotRef> localSlotOf(std::size_t transfer) const;
    void addIssue(std::size_t transfer, const SlotRef& location, IssueReason reason, std::string detail = {});
    void advance(CommitPhase phase);

    const CommitPlan& plan_;
    CommitBackend& backend_;
    ProgressFn progress_;
    CommitResult result_;
    LocalLayout current_;
    std::vector<UploadState> uploads_;
    std::map<std::size_t, PokemonPayload> received_;
    std::vector<bool> cancelledDownloads_;
    std::set<std::size_t> droppedGroups_;
    std::string saveError_;
    std::size_t totalSteps_ = 1;
    std::size_t completedSteps_ = 0;
};
