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
//
// Every phase is best-effort: one Pokemon failing (illegal upload,
// generation-incompatible local write) never rolls back or blocks the
// others in the same commit. It's simply excluded - left exactly where it
// started - while everything else proceeds. This is deliberate: the old
// all-or-nothing behavior meant a single unrelated failure after a
// successful cloud upload could discard the whole commit, leaving that
// Pokemon duplicated (a real copy on the server, and the reloaded local
// slot still showing the original too).
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
    using BankSlot = std::pair<std::uint16_t, std::uint8_t>;

    void runCommit();
    void collectCloudChanges(std::vector<UploadPokemon>& uploads, std::vector<BankSlot>& deletes);
    std::size_t countLocalChanges() const;
    std::size_t countPartyChanges() const;

    // Uploads what it can; Pokemon the server rejects are recorded in
    // result_.skipped and excluded, never retried blind. Always returns the
    // items that were actually confirmed stored.
    std::vector<UploadPokemon> runUploads(std::vector<UploadPokemon> uploads);
    void commitCloudUploadBaseline(const std::vector<UploadPokemon>& uploaded);
    void recordSkippedUpload(const UploadPokemon& upload, const std::string& reason);

    // unresolvedPayloads: exact payload bytes of every Pokemon whose cloud
    // upload was attempted this commit but failed. A local/party slot whose
    // vacated content matches one of these must NOT be cleared - its move
    // never actually completed, so clearing it would delete the only
    // remaining copy (rejected on the server, and now gone locally too).
    void runLocalWrites(std::vector<LocalWriteVerification>& verifications, bool& anyWrite, bool& anyFailure,
                         const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads);
    void runPartyWrites(std::vector<std::size_t>& slots, std::vector<std::uint16_t>& species, bool& anyWrite, bool& anyFailure,
                         const std::vector<std::vector<std::uint8_t>>& unresolvedPayloads);
    bool writeSaveAndVerify(const std::vector<LocalWriteVerification>& localVerifications,
                             const std::vector<std::size_t>& partySlots,
                             const std::vector<std::uint16_t>& partySpecies);

    // Only ever called when this commit had zero local/party write failures:
    // a delete removes the cloud original of a Pokemon that's moving
    // elsewhere, and that's only safe to do once we know its destination
    // write actually succeeded. Skipped deletes are reverted in the draft
    // (the cloud original was never actually gone).
    void runDeletes(const std::vector<BankSlot>& deletes);
    void revertCloudSlots(const std::vector<BankSlot>& slots);

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
