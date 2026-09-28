#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/CommitExecutor.hpp"
#include "bank/CommitPlan.hpp"
#include "bank/StorageController.hpp"
#include "core/AsyncJob.hpp"

#include <atomic>
#include <string>

class CommitService {
public:
    CommitService(BankContext& context, BankSession& session, StorageController& storage)
        : context_(context), session_(session), storage_(storage) {}

    void begin();
    void requestWhenIdle();
    void pumpRequest(bool canStart);
    void poll();

    bool running() const { return !job_.idle(); }
    bool requested() const { return requested_; }
    CommitPhase phase() const { return static_cast<CommitPhase>(phase_.load(std::memory_order_acquire)); }
    int progress() const { return progress_.load(std::memory_order_acquire); }

private:
    CommitSnapshot captureSnapshot();
    void runCommit();
    void applyResult();
    std::string describeLocation(const SlotRef& location) const;
    std::string describeIssue(const CommitIssue& issue) const;

    BankContext& context_;
    BankSession& session_;
    StorageController& storage_;
    AsyncJob job_;
    CommitSnapshot snapshot_;
    std::string accessToken_;
    CommitResult result_;
    bool requested_ = false;
    std::atomic<int> phase_{0};
    std::atomic<int> progress_{0};
};
