#pragma once

#include "bank/BankTypes.hpp"
#include "bank/CommitPlan.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

enum class RemoteOutcome : std::uint8_t {
    Done,
    Refused,
    Unknown
};

struct RemoteResult {
    RemoteOutcome outcome = RemoteOutcome::Unknown;
    std::string message;
};

struct CloudRejection {
    SlotRef slot;
    std::string reason;
};

struct UploadBatchResult {
    RemoteOutcome outcome = RemoteOutcome::Unknown;
    std::string message;
    std::vector<CloudRejection> rejected;
};

enum class CloudClaim : std::uint8_t {
    Claimed,
    Changed,
    Locked
};

struct ClaimBatchResult {
    RemoteOutcome outcome = RemoteOutcome::Unknown;
    std::string message;
    std::vector<CloudClaim> claims;
};

struct ClaimItem {
    SlotRef slot;
    PokemonPayload payload;
};

struct UploadItem {
    SlotRef destination;
    PokemonSummary summary;
    PokemonPayload payload;
    bool replaceOccupant = false;
};

class CommitBackend {
public:
    virtual ~CommitBackend() = default;
    virtual RemoteResult moveCloud(const SlotRef& from, const SlotRef& to) = 0;
    virtual UploadBatchResult uploadBatch(const std::vector<UploadItem>& items) = 0;
    virtual std::optional<std::array<PokemonPayload, BoxSlotCount>> readCloudBox(std::uint16_t boxKey) = 0;
    virtual RemoteResult deleteCloud(const std::vector<SlotRef>& slots) = 0;
    virtual ClaimBatchResult claimCloud(const std::vector<ClaimItem>& items) = 0;
    virtual bool writeLocal(const SlotRef& slot, const PokemonPayload& payload) = 0;
    virtual bool clearLocal(const SlotRef& slot) = 0;
    virtual bool persistLocal(std::string& error, bool finalWrite) = 0;
    virtual void discardLocal() = 0;
    virtual void log(const std::string&) {}
};

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
    std::vector<bool> cancelledDownloads_;
    std::set<std::size_t> droppedGroups_;
    std::string saveError_;
    std::size_t totalSteps_ = 1;
    std::size_t completedSteps_ = 0;
};
