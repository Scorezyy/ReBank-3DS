#include "bank/BankInputController.hpp"

#include "bank/LoadService.hpp"
#include "core/Logger.hpp"
#include "gui/BankLayout.hpp"
#include "selection/GridGeometry.hpp"

#include <algorithm>

namespace {
constexpr double InitialRepeatDelaySeconds = 0.28;
constexpr double RepeatIntervalSeconds = 0.09;
constexpr std::size_t BoxNameMaxLength = 16;

CursorDirection directionFromKeys(u32 keys) {
    if (keys & KEY_UP) {
        return CursorDirection::Up;
    }
    if (keys & KEY_DOWN) {
        return CursorDirection::Down;
    }
    if (keys & KEY_LEFT) {
        return CursorDirection::Left;
    }
    return keys & KEY_RIGHT ? CursorDirection::Right : CursorDirection::None;
}

CursorDirection directionFromCircle(circlePosition circle) {
    constexpr s16 Threshold = InputFrame::StickThreshold;
    if (circle.dy > Threshold) {
        return CursorDirection::Up;
    }
    if (circle.dy < -Threshold) {
        return CursorDirection::Down;
    }
    if (circle.dx < -Threshold) {
        return CursorDirection::Left;
    }
    return circle.dx > Threshold ? CursorDirection::Right : CursorDirection::None;
}

std::size_t neighbourSlot(const GridGeometry& grid, StoragePane pane, std::size_t slot, CursorDirection direction) {
    const int lastRow = grid.rows() - 1;
    const int lastColumn = grid.columns() - 1;
    const bool wrapsRight = pane == StoragePane::Cloud;
    const bool wrapsLeft = pane != StoragePane::Party;
    GridPoint point = grid.pointOf(slot);
    switch (direction) {
        case CursorDirection::Up:
            point.row = std::max(point.row - 1, 0);
            break;
        case CursorDirection::Down:
            point.row = std::min(point.row + 1, lastRow);
            break;
        case CursorDirection::Left:
            point.column = point.column == 0 ? (wrapsLeft ? lastColumn : 0) : point.column - 1;
            break;
        case CursorDirection::Right:
            point.column = point.column == lastColumn ? (wrapsRight ? 0 : lastColumn) : point.column + 1;
            break;
        case CursorDirection::None:
            break;
    }
    return grid.slotOf(point);
}

u64 ticksFromNow(double seconds) {
    return svcGetSystemTick() + static_cast<u64>(SYSCLOCK_ARM11 * seconds);
}
}

CursorDirection BankInputController::repeatedDirection(const InputFrame& input) {
    CursorDirection direction = directionFromKeys(input.held);
    if (direction == CursorDirection::None) {
        direction = directionFromCircle(input.circle);
    }
    if (direction == CursorDirection::None) {
        heldDirection_ = CursorDirection::None;
        directionRepeatAt_ = 0;
        return CursorDirection::None;
    }
    if (direction != heldDirection_ || directionFromKeys(input.down) == direction) {
        heldDirection_ = direction;
        directionRepeatAt_ = ticksFromNow(InitialRepeatDelaySeconds);
        return direction;
    }
    if (svcGetSystemTick() >= directionRepeatAt_) {
        directionRepeatAt_ = ticksFromNow(RepeatIntervalSeconds);
        return direction;
    }
    return CursorDirection::None;
}

void BankInputController::handle(const InputFrame& input) {
    if (commit_.running()) {
        return;
    }
    if (session_.trashConfirmVisible) {
        handleTrashConfirm(input);
        return;
    }
    if (input.pressed(KEY_B)) {
        handleBack();
        return;
    }
    if (selection_.engaged()) {
        handleSelection(input);
        return;
    }
    handleBoxShoulder(input);
    if (session_.cloudNameFocused) {
        handleCloudNameFocus(input);
        return;
    }
    if (input.pressed(KEY_START)) {
        selection_.cycleMode();
    }
    if (input.pressed(KEY_A)) {
        if (selection_.mode() != SelectionMode::Single) {
            selection_.confirm();
        } else {
            storage_.pickUpOrDrop();
        }
    }
    handleMovement(input);
    if (input.pressed(KEY_SELECT)) {
        handleCommitRequest();
    }
    pumpBackgroundWork();
    if (input.touched()) {
        handleTouch(input);
    }
}

void BankInputController::handleSelection(const InputFrame& input) {
    if (input.pressed(KEY_A)) {
        selection_.confirm();
    } else if (input.pressed(KEY_L | KEY_R)) {
        if (selection_.holding()) {
            handleBoxShoulder(input);
            selection_.followBoxChange();
        } else {
            context_.status = context_.text.get(TextId::FinishAreaFirst);
        }
    }
    handleMovement(input);
    pumpBackgroundWork();
}

void BankInputController::handleBack() {
    if (selection_.engaged()) {
        selection_.cancel();
        return;
    }
    if (session_.hand.active) {
        storage_.returnHand();
        return;
    }
    if (const auto change = storage_.pendingChange()) {
        Logger::instance().info("Discarding pending changes, first difference: " + *change);
        storage_.discardPendingChanges();
        return;
    }
    if (session_.storagePane != StoragePane::Local) {
        session_.storagePane = StoragePane::Local;
        return;
    }
    context_.leaveBank();
}

void BankInputController::handleBoxShoulder(const InputFrame& input) {
    if (!input.pressed(KEY_L | KEY_R) || session_.storagePane == StoragePane::Party) {
        return;
    }
    const bool goPrevious = input.pressed(KEY_L);
    if (session_.storagePane == StoragePane::Local) {
        const std::size_t boxCount = session_.saveAdapter.boxCount();
        storage_.persistLocalDraft();
        session_.localBox = (session_.localBox + (goPrevious ? boxCount - 1 : 1)) % boxCount;
        storage_.loadLocalBox();
        return;
    }

    const std::size_t boxLimit = context_.cloudBoxLimit();
    if (session_.trashBoxActive) {
        session_.setTrashActive(false);
        session_.cloudBox = goPrevious ? boxLimit - 1 : 0;
        storage_.refreshCloudBox();
        return;
    }
    storage_.persistCloudDraft();
    const bool leavesRange = goPrevious ? session_.cloudBox == 0 : session_.cloudBox + 1 >= boxLimit;
    if (leavesRange) {
        session_.setTrashActive(true);
        storage_.loadTrashBox();
        return;
    }
    session_.cloudBox = goPrevious ? session_.cloudBox - 1 : session_.cloudBox + 1;
    storage_.refreshCloudBox();
}

void BankInputController::handleCloudNameFocus(const InputFrame& input) {
    repeatedDirection(input);
    if (input.pressed(KEY_DOWN | KEY_B)) {
        session_.cloudNameFocused = false;
        return;
    }
    if (!input.pressed(KEY_A) || cloudSync_.renameInProgress()) {
        return;
    }
    const std::uint16_t position = session_.cloudPosition();
    const auto cached = session_.cloudBoxNames.find(position);
    const std::string current = cached != session_.cloudBoxNames.end()
        ? cached->second
        : context_.text.format(TextId::DefaultBankName, {std::to_string(position)});
    std::string edited = current;
    context_.requestText(edited, context_.text.get(TextId::BoxNameHint), BoxNameMaxLength);
    if (!edited.empty() && edited != current) {
        cloudSync_.beginRenameBox(position, edited);
    }
}

void BankInputController::handleMovement(const InputFrame& input) {
    const CursorDirection direction = repeatedDirection(input);
    if (selection_.holding()) {
        selection_.move(direction);
        return;
    }
    const std::size_t priorSlot = session_.focusedSlot;
    const StoragePane priorPane = session_.storagePane;
    if (direction != CursorDirection::None) {
        session_.focusedSlot = neighbourSlot(session_.grid(session_.storagePane), session_.storagePane, session_.focusedSlot, direction);
    }
    if (!selection_.engaged()) {
        handlePaneTransitions(directionFromKeys(input.down), priorPane, priorSlot);
    }
}

void BankInputController::enterPane(StoragePane pane, std::size_t slot) {
    session_.storagePane = pane;
    session_.focusedSlot = slot;
}

void BankInputController::handlePaneTransitions(CursorDirection pressed, StoragePane priorPane,
                                                std::size_t priorSlot) {
    const bool blockedByWall = session_.storagePane == priorPane && session_.focusedSlot == priorSlot;
    if (pressed == CursorDirection::None || !blockedByWall) {
        return;
    }
    const GridGeometry grid = session_.grid(priorPane);
    const GridPoint point = grid.pointOf(priorSlot);
    const GridGeometry cloud = session_.grid(StoragePane::Cloud);
    const GridGeometry local = session_.grid(StoragePane::Local);
    const GridGeometry party = session_.grid(StoragePane::Party);

    if (priorPane == StoragePane::Local && pressed == CursorDirection::Up && point.row == 0) {
        enterPane(StoragePane::Cloud, cloud.slotOf({cloud.rows() - 1, std::min(point.column, cloud.columns() - 1)}));
    } else if (priorPane == StoragePane::Cloud && pressed == CursorDirection::Down && point.row == grid.rows() - 1) {
        enterPane(StoragePane::Local, local.slotOf({0, std::min(point.column, local.columns() - 1)}));
    } else if (priorPane == StoragePane::Cloud && pressed == CursorDirection::Up && point.row == 0) {
        session_.cloudNameFocused = !session_.hand.active && !session_.trashBoxActive;
    } else if (priorPane == StoragePane::Local && pressed == CursorDirection::Right
               && point.column == grid.columns() - 1) {
        enterPane(StoragePane::Party, party.slotOf({std::min(point.row, party.rows() - 1), 0}));
    } else if (priorPane == StoragePane::Party && pressed == CursorDirection::Left && point.column == 0) {
        enterPane(StoragePane::Local, local.slotOf({std::min(point.row, local.rows() - 1), local.columns() - 1}));
    }
}

void BankInputController::handleCommitRequest() {
    if (session_.hand.active) {
        context_.status = context_.text.get(TextId::DropFirst);
        return;
    }
    if (!storage_.hasPendingChanges()) {
        context_.status = context_.text.get(TextId::NothingToCommit);
        return;
    }
    if (!session_.trash.empty()) {
        session_.trashConfirmVisible = true;
        return;
    }
    startCommit();
}

void BankInputController::startCommit() {
    if (context_.loads.busy() || cloudSync_.renameInProgress()) {
        commit_.requestWhenIdle();
        return;
    }
    commit_.begin();
}

void BankInputController::handleTrashConfirm(const InputFrame& input) {
    if (input.pressed(KEY_A) || input.tapped(BankLayout::TrashYesButton)) {
        session_.trashConfirmVisible = false;
        storage_.emptyTrashBox();
        startCommit();
    } else if (input.pressed(KEY_B) || input.tapped(BankLayout::TrashNoButton)) {
        session_.trashConfirmVisible = false;
    }
}

void BankInputController::pumpBackgroundWork() {
    commit_.pumpRequest(!session_.hand.active && !selection_.engaged() && !cloudSync_.renameInProgress());
    cloudSync_.pumpBackgroundFetches();
}

void BankInputController::handleTouch(const InputFrame& input) {
    const BankLayout::BoxGrid localGrid = BankLayout::localGrid(session_.grid(StoragePane::Local));
    for (std::size_t slot = 0; slot < localGrid.slotCount(); ++slot) {
        if (input.tapped(localGrid.cell(slot))) {
            activateSlot(StoragePane::Local, slot);
            return;
        }
    }
    for (std::size_t slot = 0; slot < PartySlotCount; ++slot) {
        if (input.tapped(BankLayout::partyTouchRect(slot))) {
            activateSlot(StoragePane::Party, slot);
            return;
        }
    }
}

void BankInputController::activateSlot(StoragePane pane, std::size_t slot) {
    if (session_.storagePane != pane || session_.focusedSlot != slot) {
        enterPane(pane, slot);
    } else {
        storage_.pickUpOrDrop();
    }
}
