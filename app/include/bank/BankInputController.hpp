#pragma once

#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/CommitService.hpp"
#include "bank/StorageController.hpp"

#include <3ds.h>
#include <citro2d.h>

class App;

// Translates raw buttons/circle-pad/touch input into bank-screen actions,
// delegating the actual state changes to StorageController, CloudSyncController
// and CommitService. Nothing here touches rendering.
class BankInputController {
public:
    BankInputController(App& app, BankSession& session, StorageController& storage,
                         CloudSyncController& cloudSync, CommitService& commit)
        : app_(app), session_(session), storage_(storage), cloudSync_(cloudSync), commit_(commit) {}

    void handle(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);

private:
    void handleBack();
    void handleBoxShoulder(u32 keysDown);
    void handleCloudNameFocus(u32 keysDown, u32 keysHeld, circlePosition circle);
    void handleMovement(u32 keysDown, u32 keysHeld, circlePosition circle);
    void applyMove(int direction);
    void handlePaneTransitions(u32 keysDown, StoragePane priorPane, std::size_t priorSlot);
    void handleCommitRequest();
    void handleTrashConfirm(u32 keysDown, touchPosition touch, bool touched);
    void pumpBackgroundWork();
    void handleTouch(touchPosition touch);
    bool handleTouchLocalGrid(touchPosition touch);
    bool handleTouchPartyGrid(touchPosition touch);
    int storageDirection(u32 keysDown, u32 keysHeld, circlePosition circle);

    App& app_;
    BankSession& session_;
    StorageController& storage_;
    CloudSyncController& cloudSync_;
    CommitService& commit_;
};
