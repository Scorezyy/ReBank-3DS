#pragma once

#include "bank/commit/CommitExecutor.hpp"
#include "bank/selection/SelectionState.hpp"
#include "gui/UiRenderer.hpp"
#include "i18n/Localization.hpp"

#include <3ds.h>

class BankHud {
public:
    enum class HandState { Ready, Fetching, Holding };

    BankHud(UiRenderer& ui, const Localization& text) : ui_(ui), text_(text) {}

    static u32 modeAccent(SelectionMode mode);

    void drawStatusBar(SelectionMode mode, HandState hand, bool pending);
    void drawCommitProgress(CommitPhase phase, int progress);
    void drawActionHints(bool holding, bool pending);
    void drawTrashConfirm();

private:
    UiRenderer& ui_;
    const Localization& text_;
};
