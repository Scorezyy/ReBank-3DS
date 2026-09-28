#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/CommitService.hpp"
#include "bank/StorageController.hpp"
#include "gui/InputFrame.hpp"
#include "selection/CursorDirection.hpp"
#include "selection/SelectionController.hpp"

#include <3ds.h>

class BankInputController {
public:
    BankInputController(BankContext& context, BankSession& session, StorageController& storage,
                        CloudSyncController& cloudSync, CommitService& commit, SelectionController& selection)
        : context_(context), session_(session), storage_(storage), cloudSync_(cloudSync), commit_(commit),
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
    CursorDirection repeatedDirection(const InputFrame& input);

    BankContext& context_;
    BankSession& session_;
    StorageController& storage_;
    CloudSyncController& cloudSync_;
    CommitService& commit_;
    SelectionController& selection_;
    CursorDirection heldDirection_ = CursorDirection::None;
    u64 directionRepeatAt_ = 0;
};
