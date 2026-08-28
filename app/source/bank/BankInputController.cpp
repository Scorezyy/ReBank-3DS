#include "bank/BankInputController.hpp"

#include "app/App.hpp"
#include "gui/Theme.hpp"

#include <algorithm>

int BankInputController::storageDirection(u32 keysDown, u32 keysHeld, circlePosition circle) {
    int direction = 0;
    if ((keysHeld & KEY_UP) || circle.dy > 60) {
        direction = 1;
    } else if ((keysHeld & KEY_DOWN) || circle.dy < -60) {
        direction = 2;
    } else if ((keysHeld & KEY_LEFT) || circle.dx < -60) {
        direction = 3;
    } else if ((keysHeld & KEY_RIGHT) || circle.dx > 60) {
        direction = 4;
    }

    if (direction == 0) {
        session_.heldDirection = 0;
        session_.directionRepeatAt = 0;
        return 0;
    }
    const u64 now = svcGetSystemTick();
    const bool digitalPressed = (direction == 1 && (keysDown & KEY_UP))
        || (direction == 2 && (keysDown & KEY_DOWN))
        || (direction == 3 && (keysDown & KEY_LEFT))
        || (direction == 4 && (keysDown & KEY_RIGHT));
    if (direction != session_.heldDirection || digitalPressed) {
        session_.heldDirection = direction;
        session_.directionRepeatAt = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.28);
        return direction;
    }
    if (now >= session_.directionRepeatAt) {
        session_.directionRepeatAt = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.09);
        return direction;
    }
    return 0;
}

void BankInputController::handle(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    if (commit_.running()) {
        return;
    }
    if (keysDown & KEY_B) {
        handleBack();
        return;
    }

    handleBoxShoulder(keysDown);

    if (session_.cloudNameFocused) {
        handleCloudNameFocus(keysDown, keysHeld, circle);
        return;
    }

    if (keysDown & KEY_A) {
        if (session_.hand.active) {
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

void BankInputController::handleBack() {
    if (session_.hand.active) {
        storage_.returnHand();
        return;
    }
    if (storage_.hasPendingChanges(true)) {
        storage_.discardPendingChanges();
        return;
    }
    if (session_.storagePane == StoragePane::Cloud) {
        session_.storagePane = StoragePane::Local;
    } else {
        app_.screen_ = App::Screen::GameSelect;
    }
}

void BankInputController::handleBoxShoulder(u32 keysDown) {
    if (!((keysDown & KEY_L) || (keysDown & KEY_R))) {
        return;
    }
    if (session_.hand.active) {
        app_.status_ = "Drop the Pokemon first.";
        return;
    }
    if (session_.storagePane == StoragePane::Party) {
        return;
    }

    const std::size_t boxLimit = app_.session_.boxLimit == 0 ? 50 : app_.session_.boxLimit;
    const bool goPrevious = keysDown & KEY_L;
    if (session_.storagePane == StoragePane::Cloud) {
        storage_.persistCloudDraft();
        session_.cloudBox = goPrevious
            ? (session_.cloudBox == 0 ? boxLimit - 1 : session_.cloudBox - 1)
            : (session_.cloudBox + 1) % boxLimit;
        storage_.refreshCloudBox();
    } else {
        storage_.persistLocalDraft();
        session_.localBox = goPrevious
            ? (session_.localBox == 0 ? session_.saveAdapter.boxCount() - 1 : session_.localBox - 1)
            : (session_.localBox + 1) % session_.saveAdapter.boxCount();
        storage_.loadLocalBox();
    }
}

void BankInputController::handleCloudNameFocus(u32 keysDown, u32 keysHeld, circlePosition circle) {
    storageDirection(keysDown, keysHeld, circle);
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
    app_.requestText(edited, "Box name", false);
    if (!edited.empty() && edited != current) {
        cloudSync_.beginRenameBox(position, edited);
    }
}

void BankInputController::handleMovement(u32 keysDown, u32 keysHeld, circlePosition circle) {
    const std::size_t priorSlot = session_.focusedSlot;
    const StoragePane priorPane = session_.storagePane;
    applyMove(storageDirection(keysDown, keysHeld, circle));
    handlePaneTransitions(keysDown, priorPane, priorSlot);
}

void BankInputController::applyMove(int direction) {
    if (direction == 0) {
        return;
    }
    if (session_.storagePane == StoragePane::Party) {
        std::size_t column = session_.focusedSlot % 2;
        std::size_t row = session_.focusedSlot / 2;
        if (direction == 1) {
            row = row == 0 ? 0 : row - 1;
        } else if (direction == 2) {
            row = row == 2 ? 2 : row + 1;
        } else if (direction == 3) {
            column = column == 0 ? 0 : column - 1;
        } else if (direction == 4) {
            column = column == 1 ? 1 : column + 1;
        }
        session_.focusedSlot = row * 2 + column;
        return;
    }
    std::size_t column = session_.focusedSlot % 6;
    std::size_t row = session_.focusedSlot / 6;
    if (direction == 1) {
        row = row == 0 ? 0 : row - 1;
    } else if (direction == 2) {
        row = row == 4 ? 4 : row + 1;
    } else if (direction == 3) {
        column = column == 0 ? 5 : column - 1;
    } else if (direction == 4) {
        column = (session_.storagePane == StoragePane::Local && column == 5) ? 5 : (column + 1) % 6;
    }
    session_.focusedSlot = row * 6 + column;
}

void BankInputController::handlePaneTransitions(u32 keysDown, StoragePane priorPane, std::size_t priorSlot) {
    if ((keysDown & KEY_UP)
        && priorPane == StoragePane::Local
        && session_.storagePane == StoragePane::Local
        && priorSlot < 6
        && session_.focusedSlot == priorSlot) {
        const std::size_t column = session_.focusedSlot % 6;
        session_.storagePane = StoragePane::Cloud;
        session_.focusedSlot = 24 + column;
    } else if ((keysDown & KEY_DOWN)
        && priorPane == StoragePane::Cloud
        && session_.storagePane == StoragePane::Cloud
        && priorSlot >= 24
        && session_.focusedSlot == priorSlot) {
        const std::size_t column = session_.focusedSlot % 6;
        session_.storagePane = StoragePane::Local;
        session_.focusedSlot = column;
    } else if ((keysDown & KEY_UP)
        && priorPane == StoragePane::Cloud
        && session_.storagePane == StoragePane::Cloud
        && priorSlot < 6
        && session_.focusedSlot == priorSlot
        && !session_.hand.active) {
        session_.cloudNameFocused = true;
    } else if ((keysDown & KEY_RIGHT)
        && priorPane == StoragePane::Local
        && session_.storagePane == StoragePane::Local
        && priorSlot % 6 == 5
        && session_.focusedSlot == priorSlot) {
        const std::size_t row = session_.focusedSlot / 6;
        session_.storagePane = StoragePane::Party;
        session_.focusedSlot = std::min<std::size_t>(row, 2) * 2;
    } else if ((keysDown & KEY_LEFT)
        && priorPane == StoragePane::Party
        && session_.storagePane == StoragePane::Party
        && priorSlot % 2 == 0
        && session_.focusedSlot == priorSlot) {
        const std::size_t row = session_.focusedSlot / 2;
        session_.storagePane = StoragePane::Local;
        session_.focusedSlot = row * 6 + 5;
    }
}

void BankInputController::handleCommitRequest() {
    if (session_.hand.active) {
        app_.status_ = "Drop the Pokemon first.";
    } else if (!storage_.hasPendingChanges()) {
        app_.status_ = "Nothing to commit.";
    } else if (app_.loadService_.running()) {
        commit_.requestWhenIdle();
    } else {
        commit_.begin();
    }
}

void BankInputController::pumpBackgroundWork() {
    commit_.pumpRequest();
    cloudSync_.pumpHandPayloadFetch();
    cloudSync_.pumpCloudPayloadPrefetch();
    cloudSync_.pumpCloudPrefetch();
}

void BankInputController::handleTouch(touchPosition touch) {
    if (handleTouchLocalGrid(touch)) {
        return;
    }
    handleTouchPartyGrid(touch);
}

bool BankInputController::handleTouchLocalGrid(touchPosition touch) {
    constexpr float gridX = 8.0F;
    constexpr float gridY = 60.0F;
    constexpr float pitchX = 32.0F;
    constexpr float pitchY = 25.0F;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float x = gridX + static_cast<float>(slot % 6) * pitchX;
        const float y = gridY + static_cast<float>(slot / 6) * pitchY;
        if (!UiRect{x, y, pitchX, pitchY}.contains(touch)) {
            continue;
        }
        if (session_.focusedSlot == slot && session_.storagePane == StoragePane::Local) {
            if (session_.hand.active) {
                storage_.drop();
            } else {
                storage_.pickUp();
            }
        } else {
            session_.focusedSlot = slot;
            session_.storagePane = StoragePane::Local;
        }
        return true;
    }
    return false;
}

bool BankInputController::handleTouchPartyGrid(touchPosition touch) {
    constexpr float partyColAX = 244.0F;
    constexpr float partyColBX = 288.0F;
    constexpr float partyRowStep = 45.0F;
    constexpr float partyColATop = 86.0F;
    constexpr float partyColBTop = 108.0F;
    constexpr float partyTouchHalf = 18.0F;
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const std::size_t column = slot % 2;
        const std::size_t row = slot / 2;
        const float cx = column == 0 ? partyColAX : partyColBX;
        const float cy = (column == 0 ? partyColATop : partyColBTop) + static_cast<float>(row) * partyRowStep;
        const UiRect touchRect{cx - partyTouchHalf, cy - partyTouchHalf, partyTouchHalf * 2.0F, partyTouchHalf * 2.0F};
        if (!touchRect.contains(touch)) {
            continue;
        }
        if (session_.focusedSlot == slot && session_.storagePane == StoragePane::Party) {
            if (session_.hand.active) {
                storage_.drop();
            } else {
                storage_.pickUp();
            }
        } else {
            session_.focusedSlot = slot;
            session_.storagePane = StoragePane::Party;
        }
        return true;
    }
    return false;
}
