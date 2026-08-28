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

// Owns the background "commit" job that uploads/deletes cloud Pokemon and
// writes the local save, plus the small amount of request/progress state
// the input controller and the renderer need to observe it.
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
    struct LocalWriteVerification {
        std::size_t box;
        std::size_t slot;
        std::uint16_t species;
    };

    void runCommit();
    bool collectCloudChanges(std::vector<UploadPokemon>& uploads,
                              std::vector<std::pair<std::uint16_t, std::uint8_t>>& deletes);
    std::size_t countLocalChanges() const;
    std::size_t countPartyChanges() const;
    bool runUploads(std::vector<UploadPokemon> uploads);
    void parseUploadFailure(const std::string& message, const std::vector<UploadPokemon>& batch);
    bool runLocalWrites(std::vector<LocalWriteVerification>& verifications, bool& anyWrite);
    bool runPartyWrites(std::vector<std::size_t>& slots, std::vector<std::uint16_t>& species, bool& anyWrite);
    bool writeSaveAndVerify(const std::vector<LocalWriteVerification>& localVerifications,
                             const std::vector<std::size_t>& partySlots,
                             const std::vector<std::uint16_t>& partySpecies);
    bool runDeletes(const std::vector<std::pair<std::uint16_t, std::uint8_t>>& deletes);
    void advanceProgress();
    void fail(std::string message);

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
