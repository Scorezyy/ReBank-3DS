#include "gui/screens/bank/BankHud.hpp"

#include "gui/Theme.hpp"
#include "gui/screens/bank/BankLayout.hpp"
#include "gui/widgets/Shapes.hpp"

#include <string>
#include <string_view>

using namespace Gui;

namespace {
constexpr u32 White = C2D_Color32(255, 255, 255, 255);
constexpr u32 DialogBackdrop = C2D_Color32(12, 24, 19, 255);
constexpr u32 DialogPaper = C2D_Color32(250, 247, 238, 255);
constexpr u32 IdleGlyph = C2D_Color32(20, 110, 70, 140);
constexpr float PillY = 2.0F;
constexpr float PillHeight = 16.0F;
constexpr float ModeX = 160.0F;
constexpr float ModeWidth = 78.0F;
constexpr float StartX = 246.0F;
constexpr float StartWidth = 68.0F;

TextId phaseLabel(CommitPhase phase) {
    switch (phase) {
        case CommitPhase::MovingCloud:
            return TextId::PhaseMovingCloud;
        case CommitPhase::SavingLocal:
            return TextId::PhaseSavingLocal;
        case CommitPhase::Uploading:
            return TextId::PhaseUploading;
        case CommitPhase::Finalizing:
            return TextId::PhaseFinalizing;
        case CommitPhase::RemovingCloud:
            return TextId::PhaseRemovingCloud;
        case CommitPhase::Preparing:
            break;
    }
    return TextId::PhasePreparing;
}

TextId modeLabel(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Row:
            return TextId::ModeLabelRow;
        case SelectionMode::Area:
            return TextId::ModeLabelMulti;
        case SelectionMode::Single:
            break;
    }
    return TextId::ModeLabelSingle;
}

TextId handLabel(BankHud::HandState hand) {
    switch (hand) {
        case BankHud::HandState::Fetching:
            return TextId::StateFetching;
        case BankHud::HandState::Holding:
            return TextId::StateHolding;
        case BankHud::HandState::Ready:
            break;
    }
    return TextId::StateReady;
}
}

u32 BankHud::modeAccent(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Row:
            return CursorBlue;
        case SelectionMode::Area:
            return CursorGreen;
        case SelectionMode::Single:
            break;
    }
    return CursorRed;
}

void BankHud::drawStatusBar(SelectionMode mode, HandState hand, bool pending) {
    const u32 accent = modeAccent(mode);
    C2D_DrawRectSolid(0.0F, 0.0F, 0.05F, 320.0F, 20.0F, C2D_Color32(215, 232, 224, 235));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.1F, 7.0F, accent);
    C2D_DrawRectSolid(7.0F, 9.0F, 0.15F, 14.0F, 2.0F, C2D_Color32(30, 30, 30, 255));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.2F, 2.5F, C2D_Color32(240, 240, 240, 255));

    ui_.drawText(text_.get(handLabel(hand)), 30.0F, 6.0F, 0.34F, hand == HandState::Ready ? HeaderInk : CursorGreen);
    if (pending) {
        ui_.drawText(text_.get(TextId::StatePending), 100.0F, 6.0F, 0.34F, CursorGreen);
    }

    drawRoundedRect(ModeX, PillY, ModeWidth, PillHeight, 8.0F, 0.10F, accent);
    drawRoundedRect(ModeX + 2.0F, PillY + 1.5F, ModeWidth - 4.0F, PillHeight - 3.0F, 6.5F, 0.11F,
                    C2D_Color32(255, 255, 255, 235));
    ui_.drawCentered(text_.get(modeLabel(mode)), ModeX + ModeWidth * 0.5F, PillY + 3.0F, 0.36F, accent);

    drawRoundedRect(StartX, PillY, StartWidth, PillHeight, 8.0F, 0.10F, C2D_Color32(58, 58, 58, 255));
    ui_.drawCentered("START", StartX + StartWidth * 0.5F, PillY + 3.0F, 0.32F, C2D_Color32(240, 240, 240, 255));
}

void BankHud::drawCommitProgress(CommitPhase phase, int progress) {
    C2D_DrawRectSolid(0.0F, 205.0F, 0.7F, 200.0F, 35.0F, C2D_Color32(0, 0, 0, 170));
    ui_.drawCentered(std::string(text_.get(phaseLabel(phase))) + " " + std::to_string(progress) + "%", 100.0F, 210.0F,
                     0.42F, White);
    C2D_DrawRectSolid(10.0F, 228.0F, 0.9F, 180.0F, 6.0F, C2D_Color32(50, 50, 50, 220));
    C2D_DrawRectSolid(10.0F, 228.0F, 0.92F, 180.0F * static_cast<float>(progress) / 100.0F, 6.0F, CursorGreen);
}

void BankHud::drawActionHints(bool holding, bool pending) {
    constexpr u8 FadeAlphas[] = {25, 45, 65};
    for (int band = 0; band < 3; ++band) {
        C2D_DrawRectSolid(0.0F, 210.0F + band * 6.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, FadeAlphas[band]));
    }
    C2D_DrawRectSolid(0.0F, 228.0F, 0.06F, 320.0F, 12.0F, C2D_Color32(255, 255, 255, 85));

    const u32 divider = C2D_Color32(20, 110, 70, 55);
    C2D_DrawRectSolid(72.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);
    C2D_DrawRectSolid(152.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);

    const auto hint = [&](std::string_view glyph, float x, u32 glyphColor, TextId label, u32 labelColor) {
        ui_.drawCentered(glyph, x, 219.0F, 0.42F, glyphColor);
        ui_.drawText(text_.get(label), x + 10.0F, 220.0F, 0.4F, labelColor);
    };
    hint("A", 16.0F, holding ? CursorGreen : C2D_Color32(216, 40, 32, 150), holding ? TextId::HintDrop : TextId::HintPick,
         HeaderInk);
    hint("B", 82.0F, holding ? C2D_Color32(120, 60, 160, 220) : IdleGlyph, holding ? TextId::HintReturn : TextId::Back,
         HeaderInk);
    hint("S", 162.0F, pending ? CursorGreen : IdleGlyph, TextId::HintSave, pending ? CursorGreen : HeaderInk);
}

void BankHud::drawTrashConfirm() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, DialogBackdrop);
    C2D_DrawRectSolid(18.0F, 52.0F, 0.40F, 284.0F, 148.0F, DialogPaper);
    ui_.drawCentered(text_.get(TextId::TrashCan), 160.0F, 68.0F, 0.5F, HeaderInk);
    ui_.drawCentered(text_.get(TextId::TrashConfirmMessage), 160.0F, 98.0F, 0.42F, HeaderInk);

    const auto button = [&](const UiRect& rect, u32 fill, TextId label) {
        C2D_DrawRectSolid(rect.x, rect.y, 0.46F, rect.width, rect.height, fill);
        ui_.drawCentered(text_.get(label), rect.x + rect.width * 0.5F, rect.y + 9.0F, 0.5F, White);
    };
    button(BankLayout::TrashYesButton, CursorGreen, TextId::Yes);
    button(BankLayout::TrashNoButton, Brand, TextId::No);
    ui_.drawCentered("A / B", 160.0F, 180.0F, 0.36F, HeaderInk);
}
