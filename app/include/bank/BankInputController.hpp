#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/HandController.hpp"
#include "bank/StorageController.hpp"
#include "bank/commit/CommitService.hpp"
#include "bank/selection/CursorDirection.hpp"
#include "bank/selection/DirectionRepeat.hpp"
#include "bank/selection/SelectionController.hpp"
#include "gui/InputFrame.hpp"

#include <3ds.h>

class BankInputController {
public:
    BankInputController(BankContext& context, BankSession& session, StorageController& storage, HandController& hand,
                        CloudSyncController& cloudSync, CommitService& commit, SelectionController& selection)
        : context_(context), session_(session), storage_(storage), hand_(hand), cloudSync_(cloudSync), commit_(commit),
          selection_(selection) {}

    void handle(const InputFrame& input);

private:
    void handleSelection(const InputFrame& input);
    void handleBack();
    void handleBoxShoulder(const InputFrame& input);
    void handleCloudNameFocus(const InputFrame& input);
    void handleMovement(const InputFrame& input);
    void handlePaneTransitions(CursorDirection pressed, StoragePane priorPane, std::size_t priorSlot);
    void handleCommitRequest();
    void startCommit();
    void handleTrashConfirm(const InputFrame& input);
    void pumpBackgroundWork();
    void handleTouch(const InputFrame& input);
    void activateSlot(StoragePane pane, std::size_t slot);
    void enterPane(StoragePane pane, std::size_t slot);

    BankContext& context_;
    BankSession& session_;
    StorageController& storage_;
    HandController& hand_;
    CloudSyncController& cloudSync_;
    CommitService& commit_;
    SelectionController& selection_;
    DirectionRepeat directions_;
};
