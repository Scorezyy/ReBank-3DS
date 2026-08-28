#pragma once

#include "bank/BankSession.hpp"
#include "bank/BankTypes.hpp"
#include "network/LoadService.hpp"

#include <cstdint>
#include <string>

class App;

// Owns the local/cloud/party pickup-drop-swap rules and the draft/baseline
// bookkeeping behind "pending changes". This is the storage model logic for
// the bank screen; box navigation lives in BankInputController and network
// I/O lives in CloudSyncController/CommitService.
class StorageController {
public:
    StorageController(App& app, BankSession& session) : app_(app), session_(session) {}

    void pickUp();
    void drop();
    void returnHand();
    bool hasPendingChanges(bool verbose = false) const;

    void loadLocalBox();
    void persistLocalDraft();
    void persistCloudDraft();
    void refreshCloudBox(bool keepPreviousPreview = false);
    void discardPendingChanges();

    void initializeFromOpenedGame(LoadService::OpenGameResult& result);
    void reset();

private:
    void pickUpLocal();
    void pickUpParty();
    void pickUpCloud();
    void dropLocal();
    void dropParty();
    void dropCloud();

    bool localBoxDiffers(const LocalBoxDraft& a, const LocalBoxDraft& b, std::size_t slot) const;
    bool partySlotDiffers(std::size_t slot) const;

    App& app_;
    BankSession& session_;
};
