#include "bank/BankInputController.hpp"

#include "app/App.hpp"
#include "gui/Theme.hpp"
#include "selection/GridGeometry.hpp"

#include <algorithm>

namespace {
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
    if (keys & KEY_RIGHT) {
        return CursorDirection::Right;
    }
    return CursorDirection::None;
}

CursorDirection directionFromCircle(circlePosition circle) {
    if (circle.dy > 60) {
        return CursorDirection::Up;
    }
    if (circle.dy < -60) {
        return CursorDirection::Down;
    }
    if (circle.dx < -60) {
        return CursorDirection::Left;
    }
    if (circle.dx > 60) {
        return CursorDirection::Right;
    }
    return CursorDirection::None;
}

std::size_t neighbourSlot(StoragePane pane, std::size_t slot, CursorDirection direction) {
    const GridGeometry grid = GridGeometry::forPane(pane);
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

UiRect slotTouchRect(StoragePane pane, std::size_t slot) {
    if (pane == StoragePane::Party) {
        constexpr float columnAX = 244.0F;
        constexpr float columnBX = 288.0F;
        constexpr float rowStep = 45.0F;
        constexpr float columnATop = 86.0F;
        constexpr float columnBTop = 108.0F;
        constexpr float half = 18.0F;
        const GridPoint point = GridGeometry::forPane(StoragePane::Party).pointOf(slot);
        const float cx = point.column == 0 ? columnAX : columnBX;
        const float cy = (point.column == 0 ? columnATop : columnBTop) + static_cast<float>(point.row) * rowStep;
        return UiRect{cx - half, cy - half, half * 2.0F, half * 2.0F};
    }
    constexpr float gridX = 8.0F;
    constexpr float gridY = 60.0F;
    constexpr float pitchX = 32.0F;
    constexpr float pitchY = 25.0F;
    const GridPoint point = GridGeometry::forPane(StoragePane::Local).pointOf(slot);
    return UiRect{gridX + static_cast<float>(point.column) * pitchX,
                  gridY + static_cast<float>(point.row) * pitchY, pitchX, pitchY};
}
}

CursorDirection BankInputController::repeatedDirection(u32 keysDown, u32 keysHeld, circlePosition circle) {
    CursorDirection direction = directionFromKeys(keysHeld);
    if (direction == CursorDirection::None) {
        direction = directionFromCircle(circle);
    }
    if (direction == CursorDirection::None) {
        session_.heldDirection = 0;
        session_.directionRepeatAt = 0;
        return CursorDirection::None;
    }

    const int encoded = static_cast<int>(direction);
    const u64 now = svcGetSystemTick();
    const bool pressed = directionFromKeys(keysDown) == direction;
    if (encoded != session_.heldDirection || pressed) {
        session_.heldDirection = encoded;
        session_.directionRepeatAt = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.28);
        return direction;
    }
    if (now >= session_.directionRepeatAt) {
        session_.directionRepeatAt = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.09);
        return direction;
    }
    return CursorDirection::None;
}

void BankInputController::handle(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch,
                                 bool touched) {
    if (commit_.running()) {
        return;
    }
    if (session_.trashConfirmVisible) {
        handleTrashConfirm(keysDown, touch, touched);
        return;
    }
    if (keysDown & KEY_B) {
        handleBack();
        return;
    }
    if (selection_.engaged()) {
        handleSelection(keysDown, keysHeld, circle);
        return;
    }

    handleBoxShoulder(keysDown);

    if (session_.cloudNameFocused) {
        handleCloudNameFocus(keysDown, keysHeld, circle);
        return;
    }

    if (keysDown & KEY_START) {
        selection_.cycleMode();
    }

    if (keysDown & KEY_A) {
        if (selection_.mode() != SelectionMode::Single) {
            selection_.confirm();
        } else if (session_.hand.active) {
            storage_.drop();
        } else {
            storage_.pickUp();
        }
    }

    handleMovement(keysDown, keysHeld, circle);

    if (keysDown & KEY_SELECT) {
        handleCommitRequest();
    }

    pumpBackgroundWork();

    if (touched) {
        handleTouch(touch);
    }
}

void BankInputController::handleSelection(u32 keysDown, u32 keysHeld, circlePosition circle) {
    if (keysDown & KEY_A) {
        selection_.confirm();
    } else if ((keysDown & KEY_L) || (keysDown & KEY_R)) {
        if (selection_.holding()) {
            handleBoxShoulder(keysDown);
            selection_.followBoxChange();
        } else {
            app_.status_ = "Finish or cancel the area first (B).";
        }
    }
    handleMovement(keysDown, keysHeld, circle);
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
    if (storage_.hasPendingChanges(true)) {
        storage_.discardPendingChanges();
        return;
    }
    if (session_.storagePane == StoragePane::Cloud || session_.storagePane == StoragePane::Party) {
        session_.storagePane = StoragePane::Local;
        return;
    }
    app_.screen_ = App::Screen::GameSelect;
    app_.music_.setActive(false);
}

void BankInputController::handleBoxShoulder(u32 keysDown) {
    if (!((keysDown & KEY_L) || (keysDown & KEY_R))) {
        return;
    }
    if (session_.storagePane == StoragePane::Party) {
        return;
    }

    const std::size_t boxLimit = app_.session_.boxLimit == 0 ? 50 : app_.session_.boxLimit;
    const bool goPrevious = keysDown & KEY_L;
    if (session_.storagePane != StoragePane::Cloud) {
        storage_.persistLocalDraft();
        session_.localBox = goPrevious
            ? (session_.localBox == 0 ? session_.saveAdapter.boxCount() - 1 : session_.localBox - 1)
            : (session_.localBox + 1) % session_.saveAdapter.boxCount();
        storage_.loadLocalBox();
        return;
    }

    if (session_.trashBoxActive) {
        session_.trashBoxActive = false;
        session_.trashTransitionStart = svcGetSystemTick();
        session_.cloudBox = goPrevious ? boxLimit - 1 : 0;
        storage_.refreshCloudBox();
        return;
    }

    storage_.persistCloudDraft();
    if ((goPrevious && session_.cloudBox == 0) || (!goPrevious && session_.cloudBox + 1 >= boxLimit)) {
        session_.trashBoxActive = true;
        session_.trashTransitionStart = svcGetSystemTick();
        storage_.loadTrashBox();
        return;
    }
    session_.cloudBox = goPrevious ? session_.cloudBox - 1 : session_.cloudBox + 1;
    storage_.refreshCloudBox();
}

void BankInputController::handleCloudNameFocus(u32 keysDown, u32 keysHeld, circlePosition circle) {
    repeatedDirection(keysDown, keysHeld, circle);
    if (keysDown & (KEY_DOWN | KEY_B)) {
        session_.cloudNameFocused = false;
        return;
    }
    if (!(keysDown & KEY_A) || cloudSync_.renameInProgress()) {
        return;
    }
    const auto position = static_cast<std::uint16_t>(session_.cloudBox + 1);
    const auto cached = session_.cloudBoxNames.find(position);
    const std::string current = cached != session_.cloudBoxNames.end()
        ? cached->second
        : ("Bank " + std::to_string(position));
    std::string edited = current;
    app_.requestText(edited, "Box name", false, 16);
    if (!edited.empty() && edited != current) {
        cloudSync_.beginRenameBox(position, edited);
    }
}

void BankInputController::handleMovement(u32 keysDown, u32 keysHeld, circlePosition circle) {
    const CursorDirection direction = repeatedDirection(keysDown, keysHeld, circle);
    if (selection_.holding()) {
        selection_.move(direction);
        return;
    }

    const std::size_t priorSlot = session_.focusedSlot;
    const StoragePane priorPane = session_.storagePane;
    moveFocus(direction);
    if (selection_.engaged()) {
        return;
    }
    handlePaneTransitions(directionFromKeys(keysDown), priorPane, priorSlot);
}

void BankInputController::moveFocus(CursorDirection direction) {
    if (direction == CursorDirection::None) {
        return;
    }
    session_.focusedSlot = neighbourSlot(session_.storagePane, session_.focusedSlot, direction);
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

    const GridGeometry grid = GridGeometry::forPane(priorPane);
    const GridPoint point = grid.pointOf(priorSlot);
    const bool onTopRow = point.row == 0;
    const bool onBottomRow = point.row == grid.rows() - 1;
    const bool onRightColumn = point.column == grid.columns() - 1;
    const bool onLeftColumn = point.column == 0;

    if (priorPane == StoragePane::Local && pressed == CursorDirection::Up && onTopRow) {
        const GridGeometry cloud = GridGeometry::forPane(StoragePane::Cloud);
        enterPane(StoragePane::Cloud, cloud.slotOf(GridPoint{cloud.rows() - 1, point.column}));
        return;
    }
    if (priorPane == StoragePane::Cloud && pressed == CursorDirection::Down && onBottomRow) {
        const GridGeometry local = GridGeometry::forPane(StoragePane::Local);
        enterPane(StoragePane::Local, local.slotOf(GridPoint{0, point.column}));
        return;
    }
    if (priorPane == StoragePane::Cloud && pressed == CursorDirection::Up && onTopRow
        && !session_.hand.active && !session_.trashBoxActive) {
        session_.cloudNameFocused = true;
        return;
    }
    if (priorPane == StoragePane::Local && pressed == CursorDirection::Right && onRightColumn) {
        const GridGeometry party = GridGeometry::forPane(StoragePane::Party);
        enterPane(StoragePane::Party, party.slotOf(GridPoint{std::min(point.row, party.rows() - 1), 0}));
        return;
    }
    if (priorPane == StoragePane::Party && pressed == CursorDirection::Left && onLeftColumn) {
        const GridGeometry local = GridGeometry::forPane(StoragePane::Local);
        enterPane(StoragePane::Local, local.slotOf(GridPoint{point.row, local.columns() - 1}));
    }
}

void BankInputController::handleCommitRequest() {
    if (session_.hand.active) {
        app_.status_ = "Drop the Pokemon first.";
        return;
    }
    if (!storage_.hasPendingChanges()) {
        app_.status_ = "Nothing to commit.";
        return;
    }
    if (!session_.trashBox.empty()) {
        session_.trashConfirmVisible = true;
        return;
    }
    if (app_.loadService_.running()) {
        commit_.requestWhenIdle();
        return;
    }
    commit_.begin();
}

void BankInputController::handleTrashConfirm(u32 keysDown, touchPosition touch, bool touched) {
    constexpr UiRect yesButton{40.0F, 130.0F, 100.0F, 34.0F};
    constexpr UiRect noButton{180.0F, 130.0F, 100.0F, 34.0F};
    const bool touchYes = touched && (keysDown & KEY_TOUCH) && yesButton.contains(touch);
    const bool touchNo = touched && (keysDown & KEY_TOUCH) && noButton.contains(touch);
    if ((keysDown & KEY_A) || touchYes) {
        session_.trashConfirmVisible = false;
        storage_.emptyTrashBox();
        if (app_.loadService_.running()) {
            commit_.requestWhenIdle();
        } else {
            commit_.begin();
        }
        return;
    }
    if ((keysDown & KEY_B) || touchNo) {
        session_.trashConfirmVisible = false;
    }
}

void BankInputController::pumpBackgroundWork() {
    commit_.pumpRequest();
    cloudSync_.pumpHandPayloadFetch();
    cloudSync_.pumpHeldRegionPayloadFetch();
    cloudSync_.pumpCloudPayloadPrefetch();
    cloudSync_.pumpCloudPrefetch();
}

void BankInputController::handleTouch(touchPosition touch) {
    if (handleTouchGrid(touch, StoragePane::Local)) {
        return;
    }
    handleTouchGrid(touch, StoragePane::Party);
}

bool BankInputController::handleTouchGrid(touchPosition touch, StoragePane pane) {
    const std::size_t slotCount = GridGeometry::forPane(pane).slotCount();
    for (std::size_t slot = 0; slot < slotCount; ++slot) {
        if (!slotTouchRect(pane, slot).contains(touch)) {
            continue;
        }
        activateSlot(pane, slot);
        return true;
    }
    return false;
}

void BankInputController::activateSlot(StoragePane pane, std::size_t slot) {
    if (session_.storagePane != pane || session_.focusedSlot != slot) {
        session_.storagePane = pane;
        session_.focusedSlot = slot;
        return;
    }
    if (session_.hand.active) {
        storage_.drop();
        return;
    }
    storage_.pickUp();
}
