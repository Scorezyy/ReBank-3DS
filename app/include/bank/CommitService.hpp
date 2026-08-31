#pragma once

#include "bank/BankSession.hpp"
#include "bank/BankTypes.hpp"
#include "bank/StorageController.hpp"
#include "core/AsyncJob.hpp"
#include "network/ApiClient.hpp"

#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

class App;

class CommitService {
public:
    CommitService(App& app, BankSession& session, StorageController& storage)
        : app_(app), session_(session), storage_(storage) {}

    void begin();
    void requestWhenIdle();
    void pumpRequest();
    void poll();

    bool running() const { return job_.running(); }
    bool requested() const { return requested_; }
    int phase() const { return phase_.load(std::memory_order_acquire); }
    int progress() const { return progress_.load(std::memory_order_acquire); }

private:
    using BankSlot = std::pair<std::uint16_t, std::uint8_t>;

    void runCommit();
    void collectCloudChanges(std::vector<UploadPokemon>& uploads, std::vector<BankSlot>& deletes);
    std::size_t countLocalChanges() const;
    std::size_t countPartyChanges() const;

    std::vector<UploadPokemon> runUploads(std::vector<UploadPokemon> uploads);
    void commitCloudUploadBaseline(const std::vector<UploadPokemon>& uploaded);
    void recordSkippedUpload(const UploadPokemon& upload, const std::string& reason);

    bool clearLocalSlot(std::size_t boxKey, std::size_t slot, const LocalBoxDraft& baseline,
                         const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads);
    bool writeLocalSlot(std::size_t boxKey, std::size_t slot, const LocalBoxDraft& draft,
                         const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads);
    void runLocalWrites(bool& anyWrite, bool& anyFailure,
                         const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads,
                         const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads);
    bool clearPartySlotChecked(std::size_t slot, const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads);
    bool writePartySlot(std::size_t slot, const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads);
    void runPartyWrites(bool& anyWrite, bool& anyFailure,
                         const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads,
                         const std::vector<std::vector<std::uint8_t>>& stillOnCloudPayloads);
    bool writeSaveAndVerify();

    void runDeletes(const std::vector<BankSlot>& deletes);
    void revertCloudSlots(const std::vector<BankSlot>& slots);
    void revertLocalSlot(std::size_t boxKey, std::size_t slot, const LocalBoxDraft& baseline);

    void advanceProgress();

    App& app_;
    BankSession& session_;
    StorageController& storage_;
    AsyncJob job_;
    CommitResult result_;
    bool requested_ = false;
    std::atomic<int> phase_{0};
    std::atomic<int> progress_{0};
    std::size_t totalSteps_ = 0;
    std::size_t completedSteps_ = 0;
};
