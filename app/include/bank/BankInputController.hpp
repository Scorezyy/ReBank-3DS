#pragma once

#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/CommitService.hpp"
#include "bank/StorageController.hpp"
#include "selection/CursorDirection.hpp"
#include "selection/SelectionController.hpp"

#include <3ds.h>
#include <citro2d.h>

class App;

// Translates raw buttons/circle-pad/touch input into bank-screen actions,
// delegating the actual state changes to StorageController, CloudSyncController,
// CommitService and SelectionController. Nothing here touches rendering.
class BankInputController {
public:
    BankInputController(App& app, BankSession& session, StorageController& storage,
                         CloudSyncController& cloudSync, CommitService& commit, SelectionController& selection)
        : app_(app), session_(session), storage_(storage), cloudSync_(cloudSync), commit_(commit),
          selection_(selection) {}

    void handle(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);

private:
    void handleSelection(u32 keysDown, u32 keysHeld, circlePosition circle);
    void handleBack();
    void handleBoxShoulder(u32 keysDown);
    void handleCloudNameFocus(u32 keysDown, u32 keysHeld, circlePosition circle);
    void handleMovement(u32 keysDown, u32 keysHeld, circlePosition circle);
    void moveFocus(CursorDirection direction);
    void handlePaneTransitions(CursorDirection pressed, StoragePane priorPane, std::size_t priorSlot);
    void enterPane(StoragePane pane, std::size_t slot);
    void handleCommitRequest();
    void handleTrashConfirm(u32 keysDown, touchPosition touch, bool touched);
    void pumpBackgroundWork();
    void handleTouch(touchPosition touch);
    bool handleTouchGrid(touchPosition touch, StoragePane pane);
    void activateSlot(StoragePane pane, std::size_t slot);
    CursorDirection repeatedDirection(u32 keysDown, u32 keysHeld, circlePosition circle);

    App& app_;
    BankSession& session_;
    StorageController& storage_;
    CloudSyncController& cloudSync_;
    CommitService& commit_;
    SelectionController& selection_;
};
