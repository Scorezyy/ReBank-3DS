#pragma once

#include "bank/BankSession.hpp"
#include "bank/CommitService.hpp"
#include "network/RenameController.hpp"

#include <cstdint>
#include <string>

class App;

class CloudSyncController {
public:
    CloudSyncController(App& app, BankSession& session, CommitService& commit)
        : app_(app), session_(session), commit_(commit) {}

    void pumpHandPayloadFetch();
    void pumpCloudPayloadPrefetch();
    void pumpCloudPrefetch();

    void onCloudBoxLoaded();
    void onCloudPickupCompleted();
    void onCloudSwapCompleted();

    void beginRenameBox(std::uint16_t position, std::string name);
    void pollRenameBox();
    bool renameInProgress() const { return renameController_.isRunning(); }

private:
    bool nextCloudPrefetchKey(std::uint16_t& outKey) const;
    bool blockedByOtherWork() const;

    App& app_;
    BankSession& session_;
    CommitService& commit_;
    RenameController renameController_;
};
