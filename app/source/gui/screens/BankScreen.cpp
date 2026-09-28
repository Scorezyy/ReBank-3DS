#include "gui/screens/BankScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "gui/elements/BoxBackground.hpp"
#include "gui/elements/Cursor.hpp"
#include "gui/elements/PokemonBadges.hpp"
#include "gui/elements/Shapes.hpp"
#include "save/catalog/GameCatalog.hpp"
#include "selection/GridGeometry.hpp"

#include <enums/Species.hpp>
#include <utils/i18n.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

using namespace Gui;
using BankLayout::CloudGrid;

namespace {
constexpr auto Lang = pksm::Language::ENG;
constexpr float InfoTop = 8.0F;
constexpr float InfoRowHeight = 15.0F;
constexpr float InfoLeft = CloudGrid.right();
constexpr float InfoWidth = 400.0F - InfoLeft;
constexpr float InfoCenterX = InfoLeft + InfoWidth * 0.5F;
constexpr float InfoValueRight = 400.0F - 8.0F;
constexpr std::string_view Placeholder = "-----";
constexpr u32 White = C2D_Color32(255, 255, 255, 255);
constexpr u32 DimTint = C2D_Color32(72, 72, 72, 255);
constexpr u32 DialogBackdrop = C2D_Color32(12, 24, 19, 255);
constexpr u32 DialogPaper = C2D_Color32(250, 247, 238, 255);

double secondsNow() {
    return static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
}

TextId commitPhaseLabel(CommitPhase phase) {
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

bool incompatibleWithSave(const PokemonSummary& pokemon, std::uint8_t saveGeneration) {
    const GameDescriptor* origin = findGame(pokemon.gameCode);
    const std::uint8_t format = pokemon.format != 0
        ? pokemon.format
        : (origin ? static_cast<std::uint8_t>(origin->format) : 0);
    return saveGeneration != 0 && format != 0 && format > saveGeneration;
}

void drawStretched(C2D_SpriteSheet sheet, float x, float y, float z, float width, float height) {
    const C2D_Image image = C2D_SpriteSheetGetImage(sheet, 0);
    C2D_DrawImageAt(image, x, y, z, nullptr, width / static_cast<float>(image.subtex->width),
                    height / static_cast<float>(image.subtex->height));
}
}

BankScreen::BankScreen(App& app)
    : Screen(app),
      context_{app_.api_, app_.loadService_, app_.session_, app_.status_, app_.errors_, app_.localization_,
               [this]() { leaveBank(); },
               [this](std::string& value, std::string_view hint, std::size_t maxLength) {
                   app_.requestText(value, hint, false, maxLength);
               }},
      session_(app_.saveAdapter_),
      storage_(context_, session_),
      commit_(context_, session_, storage_),
      selection_(context_, session_, storage_),
      cloudSync_(context_, session_, storage_, commit_, selection_),
      input_(context_, session_, storage_, cloudSync_, commit_, selection_) {}

void BankScreen::pollBackgroundWork() {
    commit_.poll();
    cloudSync_.pollRenameBox();
}

void BankScreen::leaveBank() {
    app_.screen_ = ScreenId::GameSelect;
    app_.music_.setActive(false);
}

void BankScreen::pollCartridgeSlot() {
    if (commit_.running()) {
        return;
    }
    if (!session_.saveAdapter.isCartridge()) {
        cartridgeSlot_.forget();
        return;
    }
    if (cartridgeSlot_.poll() != CartridgeSlotMonitor::Change::Removed) {
        return;
    }
    Logger::instance().warning("Cartridge removed while banking, returning to game select");
    storage_.reset();
    cartridgeSlot_.forget();
    app_.status_.clear();
    leaveBank();
    app_.showError(TextId::CartridgeRemovedTitle, TextId::CartridgeRemovedMessage);
}

void BankScreen::update(const InputFrame& input) {
    pollCartridgeSlot();
    if (app_.screen_ == ScreenId::Bank) {
        input_.handle(input);
    }
}

void BankScreen::onGameOpened() {
    storage_.initializeFromOpenedGame(app_.saveLoadService_.openGameResult, app_.cloudBoxCache_,
                                      app_.cloudBoxNamesCache_);
    app_.screen_ = ScreenId::Bank;
    const auto games = supportedGames();
    const std::size_t catalogIndex = app_.saveLoadService_.catalogIndex;
    app_.music_.setActive(catalogIndex < games.size()
                          && games[catalogIndex].platform == GamePlatform::VirtualConsole);
}

bool BankScreen::focused(StoragePane pane, std::size_t slot) const {
    return session_.storagePane == pane && session_.focusedSlot == slot;
}

const PokemonSummary& BankScreen::focusedSummary() const {
    return storage_.slots().peek(session_.focusedAddress(), session_.focusedSlot);
}

bool BankScreen::fetchingHandPayload() const {
    const LoadService& loads = app_.loadService_;
    const Hand& hand = session_.hand;
    const bool awaitingHand = hand.active && hand.source.isCloudBank() && !hand.payloadKnown
        && loads.operation() == LoadService::Operation::PickupCloud;
    return loads.running() && (awaitingHand || loads.operation() == LoadService::Operation::SwapCloud);
}

u32 BankScreen::selectionModeAccent() const {
    switch (selection_.mode()) {
        case SelectionMode::Row:
            return CursorBlue;
        case SelectionMode::Area:
            return CursorGreen;
        case SelectionMode::Single:
            break;
    }
    return CursorRed;
}

std::optional<C2D_Image> BankScreen::spriteOf(const PokemonSummary& summary) const {
    if (!summary.occupied() || !app_.resources_.pokemonSprites) {
        return std::nullopt;
    }
    return C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, summary.species);
}

void BankScreen::drawSprite(const PokemonSummary& summary, float cx, float cy, float z, float badgeZ, bool dimmed,
                            float xShift) const {
    const std::optional<C2D_Image> sprite = spriteOf(summary);
    if (!sprite) {
        return;
    }
    const C2D_Image image = *sprite;
    const float width = image.subtex->width;
    const float height = image.subtex->height;
    C2D_ImageTint tint{};
    if (dimmed) {
        C2D_PlainImageTint(&tint, DimTint, 0.82F);
    }
    C2D_DrawImageAt(image, std::round(cx - width * 0.5F + xShift), std::round(cy - height * 0.5F), z,
                    dimmed ? &tint : nullptr);
    drawPokemonBadges(app_.resources_.overlayIcons, summary, cx, cy, width * 0.5F, height * 0.5F, badgeZ);
}

void BankScreen::drawCarriedSprite(const PokemonSummary& summary, float cx, float cy, float z) const {
    const std::optional<C2D_Image> sprite = spriteOf(summary);
    if (!sprite) {
        return;
    }
    const C2D_Image image = *sprite;
    const float width = image.subtex->width;
    const float height = image.subtex->height;
    const float shadowWidth = width * 0.6F;
    const float shadowHeight = shadowWidth * 0.35F;
    C2D_DrawEllipseSolid(cx - shadowWidth * 0.5F, cy + height * 0.42F - shadowHeight * 0.5F, z - 0.02F, shadowWidth,
                         shadowHeight, C2D_Color32(20, 20, 20, 90));
    constexpr float Lift = 8.0F;
    const float spriteY = cy - Lift;
    C2D_DrawImageAt(image, std::round(cx - width * 0.5F), std::round(spriteY - height * 0.5F), z);
    drawPokemonBadges(app_.resources_.overlayIcons, summary, cx, spriteY, width * 0.5F, height * 0.5F, z + 0.01F);
}

void BankScreen::drawHeldRegion(StorageAddress address, std::size_t slot, float cx, float cy, float halfWidth,
                                float halfHeight) const {
    const auto held = selection_.heldSummaryAt(address, slot);
    if (!held) {
        return;
    }
    C2D_DrawRectSolid(cx - halfWidth, cy - halfHeight, SelectionHighlightDepth, halfWidth * 2.0F, halfHeight * 2.0F,
                      SelectionHighlightFill);
    drawCarriedSprite(*held, cx, cy, HeldRegionSpriteDepth);
}

void BankScreen::drawFocusCursor(float cx, float cy, float cursorYOffset, float radius, float height) const {
    const bool carrying = session_.hand.active || selection_.holding();
    drawBouncingCursor(cx, cy - cursorYOffset, radius, height, carrying ? CursorGreen : selectionModeAccent());
    if (session_.hand.active) {
        drawCarriedSprite(session_.hand.summary, cx, cy, 0.6F);
    }
}

void BankScreen::drawGridCorners(const BankLayout::BoxGrid& grid) const {
    drawPlusMark(grid.left - 3.0F, grid.top - 3.0F, HeaderInk);
    drawPlusMark(grid.right() + 3.0F, grid.top - 3.0F, HeaderInk);
    drawPlusMark(grid.left - 3.0F, grid.bottom() + 3.0F, HeaderInk);
    drawPlusMark(grid.right() + 3.0F, grid.bottom() + 3.0F, HeaderInk);
}

void BankScreen::drawMarkedCells(StoragePane pane) const {
    if (session_.storagePane != pane) {
        return;
    }
    for (const std::size_t slot : selection_.markedSlots()) {
        if (pane == StoragePane::Party) {
            const BankLayout::Point center = BankLayout::partyTileCenter(slot);
            const float half = BankLayout::PartyTileSize * 0.5F;
            C2D_DrawRectSolid(center.x - half, center.y - half, SelectionHighlightDepth, half * 2.0F, half * 2.0F,
                              SelectionHighlightFill);
            continue;
        }
        const UiRect cell = (pane == StoragePane::Cloud ? CloudGrid
                                                        : BankLayout::localGrid(session_.grid(pane))).cell(slot);
        C2D_DrawRectSolid(cell.x, cell.y, SelectionHighlightDepth, cell.width, cell.height, SelectionHighlightFill);
    }
}

void BankScreen::renderTop(float eyeOffset) {
    constexpr double TransitionSeconds = 0.4;
    const double since = secondsNow() - static_cast<double>(session_.trashTransitionStart) / SYSCLOCK_ARM11;
    const float step = static_cast<float>(std::clamp(since / TransitionSeconds, 0.0, 1.0));
    drawBoxBackground(app_.resources_.boxBackground, session_.trashBoxActive ? step : 1.0F - step);
    renderTopHeader();
    renderCloudGrid(eyeOffset);
    renderInfoPanel();
}

void BankScreen::renderTopHeader() {
    constexpr float BarWidth = 200.0F * BankLayout::TopScale;
    constexpr float BarY = 6.0F;
    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 8.0F, BarY, 0.14F, nullptr,
                        BankLayout::TopScale, 1.0F);
    } else {
        drawPill(8.0F, BarY, BarWidth, 30.0F, 0.14F, BoxPlateBorder);
        drawPill(10.0F, BarY + 1.0F, BarWidth - 4.0F, 27.0F, 0.15F, BoxPlate);
    }
    if (session_.cloudNameFocused) {
        C2D_DrawRectSolid(8.0F, BarY - 1.0F, 0.145F, BarWidth, 2.0F, CursorGreen);
        C2D_DrawRectSolid(8.0F, BarY + 25.0F, 0.145F, BarWidth, 2.0F, CursorGreen);
        drawBouncingCursor(8.0F + BarWidth * 0.5F, BarY - 6.0F, 3.5F, 12.0F, CursorRed);
    }
    if (session_.trashBoxActive) {
        ui().drawCentered(text(TextId::TrashCan), 8.0F + BarWidth * 0.5F, BarY + 5.0F, 0.55F, HeaderInk);
        return;
    }
    const auto cachedName = session_.cloudBoxNames.find(session_.cloudPosition());
    const std::string label = cachedName != session_.cloudBoxNames.end()
        ? cachedName->second
        : format(TextId::DefaultBankName, {std::to_string(session_.cloudPosition())});
    ui().drawCentered(label, 8.0F + BarWidth * 0.5F, BarY + 5.0F, 0.55F, HeaderInk);

    const LoadService& loads = app_.loadService_;
    const bool loadingThisBox = loads.running() && loads.operation() == LoadService::Operation::CloudBox
        && loads.cloudBoxKey == session_.cloudKey();
    const bool waitingOnHeldRegion = selection_.holding() && selection_.nextPayloadRequest().has_value();
    if (loadingThisBox || fetchingHandPayload() || waitingOnHeldRegion) {
        const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(secondsNow()) * 6.0F);
        C2D_DrawCircleSolid(312.0F, BarY + 15.0F, 0.4F, 3.0F + pulse * 2.0F, HeaderInk);
        ui().drawText(text(TextId::BankLoading), 320.0F, BarY + 9.0F, 0.34F, HeaderInk);
    }
}

void BankScreen::renderCloudGrid(float eyeOffset) {
    drawGridCorners(CloudGrid);
    const StorageAddress address{StoragePane::Cloud, session_.trashBoxActive};
    const std::uint8_t saveGeneration = session_.saveAdapter.gameGeneration();
    for (std::size_t slot = 0; slot < BoxSlotCount; ++slot) {
        const float cx = CloudGrid.centerX(slot);
        const float cy = CloudGrid.centerY(slot);
        const PokemonSummary& pokemon = storage_.slots().peek(address, slot);
        drawSprite(pokemon, cx, cy, 0.3F, 0.31F, incompatibleWithSave(pokemon, saveGeneration), eyeOffset * 0.2F);
        drawHeldRegion(address, slot, cx, cy, CloudGrid.pitchX * 0.5F, CloudGrid.pitchY * 0.5F);
        if (focused(StoragePane::Cloud, slot) && !session_.cloudNameFocused) {
            drawFocusCursor(cx, cy, 22.0F, 3.5F, 12.0F);
        }
    }
    drawMarkedCells(StoragePane::Cloud);
}

void BankScreen::drawInfoHeader(const PokemonSummary& focused, const std::string& speciesName) {
    const std::string header = "#" + std::to_string(focused.species) + " "
        + (speciesName.empty() ? focused.nickname : speciesName);
    if (app_.resources_.nameDexPlate) {
        const C2D_Image plate = C2D_SpriteSheetGetImage(app_.resources_.nameDexPlate, 0);
        C2D_DrawImageAt(plate, InfoLeft, InfoTop - 4.0F, 0.13F, nullptr,
                        InfoWidth / static_cast<float>(plate.subtex->width), 0.85F);
    }
    ui().drawCentered(header, InfoCenterX, InfoTop, 0.46F, app_.resources_.nameDexPlate ? DialogPaper : HeaderInk);

    const C2D_SpriteSheet genderSheet = focused.gender == pksm::Gender::Male
        ? app_.resources_.genderMaleIcon
        : (focused.gender == pksm::Gender::Female ? app_.resources_.genderFemaleIcon : nullptr);
    if (!genderSheet) {
        return;
    }
    const C2D_Image icon = C2D_SpriteSheetGetImage(genderSheet, 0);
    if (!icon.tex) {
        return;
    }
    constexpr float GenderScale = 0.8F;
    const float iconWidth = static_cast<float>(icon.subtex->width) * GenderScale;
    const float iconHeight = static_cast<float>(icon.subtex->height) * GenderScale;
    const float iconX = std::min(InfoCenterX + ui().textWidth(header, 0.46F) * 0.5F + 4.0F, 400.0F - 4.0F - iconWidth);
    C2D_DrawImageAt(icon, iconX, InfoTop + 9.0F - iconHeight * 0.5F, 0.15F, nullptr, GenderScale, GenderScale);
}

void BankScreen::drawInfoStripe(const InfoRow& row) const {
    if (row.index % 2 == 0 && app_.resources_.infoStripe) {
        drawStretched(app_.resources_.infoStripe, InfoLeft, row.y, 0.12F, InfoWidth, InfoRowHeight);
    }
}

void BankScreen::drawInfoRow(InfoRow& row, std::string_view label, std::string_view value, float fontSize) {
    drawInfoStripe(row);
    if (app_.resources_.pointSmall) {
        const C2D_Image dot = C2D_SpriteSheetGetImage(app_.resources_.pointSmall, 0);
        C2D_DrawImageAt(dot, InfoLeft + 6.0F,
                        row.y + InfoRowHeight * 0.5F - static_cast<float>(dot.subtex->height) * 0.5F, 0.13F);
    }
    ui().drawText(label, InfoLeft + 16.0F, row.y + 2.0F, fontSize, HeaderInk);
    ui().drawRight(value.empty() ? Placeholder : value, InfoValueRight, row.y + 2.0F, fontSize, HeaderInk);
    row.y += InfoRowHeight;
    ++row.index;
}

void BankScreen::drawTypeRow(InfoRow& row, const PokemonSummary& focused) {
    const C2D_SpriteSheet sheet = app_.resources_.typeBanners;
    drawInfoStripe(row);
    ui().drawText(text(TextId::InfoType), InfoLeft + 16.0F, row.y + 2.0F, 0.40F, HeaderInk);

    constexpr float TargetHeight = 12.0F;
    constexpr float Gap = 4.0F;
    const float nativeHeight = typeBannerHeight(sheet, focused.type1);
    const float scale = nativeHeight > 0.0F ? TargetHeight / nativeHeight : 1.0F;
    const float firstWidth = typeBannerWidth(sheet, focused.type1, scale);
    const float bannerY = row.y + (InfoRowHeight - nativeHeight * scale) * 0.5F;
    if (focused.type1 == focused.type2) {
        drawTypeBanner(sheet, focused.type1, InfoValueRight - firstWidth, bannerY, 0.32F, scale);
    } else {
        const float left = InfoValueRight - (firstWidth + Gap + typeBannerWidth(sheet, focused.type2, scale));
        drawTypeBanner(sheet, focused.type1, left, bannerY, 0.32F, scale);
        drawTypeBanner(sheet, focused.type2, left + firstWidth + Gap, bannerY, 0.32F, scale);
    }
    row.y += InfoRowHeight;
    ++row.index;
}

void BankScreen::renderInfoPanel() {
    const PokemonSummary& focused = focusedSummary();
    if (!focused.occupied()) {
        const TextId empty = session_.storagePane == StoragePane::Cloud ? TextId::EmptyCloudSlot
            : (session_.storagePane == StoragePane::Party ? TextId::EmptyPartySlot : TextId::EmptySlot);
        ui().drawCentered(text(empty), InfoCenterX, InfoTop + 16.0F, 0.4F, HeaderInk);
        return;
    }
    const std::string speciesName = pksm::Species(focused.species).localize(Lang);
    drawInfoHeader(focused, speciesName);

    InfoRow row{InfoTop + 18.0F, 0};
    drawInfoRow(row, text(TextId::InfoLevel), std::to_string(focused.level), 0.40F);
    drawInfoRow(row, text(TextId::InfoTrainer), focused.trainerName, 0.40F);
    if (!focused.nickname.empty() && focused.nickname != speciesName) {
        drawInfoRow(row, text(TextId::InfoNickname), focused.nickname, 0.40F);
    }
    drawInfoRow(row, text(TextId::InfoItem), focused.heldItem != 0 ? i18n::item(Lang, focused.heldItem) : "", 0.38F);
    drawInfoRow(row, text(TextId::InfoAbility), i18n::ability(Lang, focused.ability), 0.38F);
    drawInfoRow(row, text(TextId::InfoNature), i18n::nature(Lang, focused.nature), 0.38F);
    if (focused.language != pksm::Language::None) {
        drawInfoRow(row, text(TextId::InfoLanguage), i18n::langString(focused.language), 0.34F);
    }
    drawTypeRow(row, focused);
    if (const std::string origin = i18n::game(Lang, focused.originGame); !origin.empty()) {
        drawInfoRow(row, text(TextId::InfoOrigin), origin, 0.40F);
    }
    row.y += 4.0F;
    for (const pksm::Move& move : focused.moves) {
        drawInfoRow(row, text(TextId::InfoMove), move == pksm::Move::None ? "" : i18n::move(Lang, move), 0.38F);
        row.y += 1.0F;
    }
}

void BankScreen::render() {
    if (session_.trashConfirmVisible) {
        renderTrashConfirmDialog();
        return;
    }
    drawLinePattern(app_.resources_.bottomBackground, C2D_Color32(158, 224, 152, 255));
    renderStatusBar();
    renderLocalBoxHeader();
    renderLocalGrid();
    renderTeamHeader();
    renderPartyGrid();
    if (commit_.running()) {
        renderCommitOverlay();
    } else {
        renderActionHints();
    }
}

void BankScreen::renderStatusBar() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.05F, 320.0F, 20.0F, C2D_Color32(215, 232, 224, 235));
    const u32 accent = selectionModeAccent();
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.1F, 7.0F, accent);
    C2D_DrawRectSolid(7.0F, 9.0F, 0.15F, 14.0F, 2.0F, C2D_Color32(30, 30, 30, 255));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.2F, 2.5F, C2D_Color32(240, 240, 240, 255));

    const bool fetching = fetchingHandPayload();
    const bool holding = session_.hand.active;
    const TextId state = fetching ? TextId::StateFetching : (holding ? TextId::StateHolding : TextId::StateReady);
    ui().drawText(text(state), 30.0F, 6.0F, 0.34F, fetching || holding ? CursorGreen : HeaderInk);
    if (storage_.hasPendingChanges()) {
        ui().drawText(text(TextId::StatePending), 100.0F, 6.0F, 0.34F, CursorGreen);
    }

    constexpr float PillY = 2.0F;
    constexpr float PillHeight = 16.0F;
    constexpr float ModeX = 160.0F;
    constexpr float ModeWidth = 78.0F;
    drawRoundedRect(ModeX, PillY, ModeWidth, PillHeight, 8.0F, 0.10F, accent);
    drawRoundedRect(ModeX + 2.0F, PillY + 1.5F, ModeWidth - 4.0F, PillHeight - 3.0F, 6.5F, 0.11F,
                    C2D_Color32(255, 255, 255, 235));
    const TextId modeLabel = selection_.mode() == SelectionMode::Row ? TextId::ModeLabelRow
        : (selection_.mode() == SelectionMode::Area ? TextId::ModeLabelMulti : TextId::ModeLabelSingle);
    ui().drawCentered(text(modeLabel), ModeX + ModeWidth * 0.5F, PillY + 3.0F, 0.36F, accent);

    constexpr float StartX = 246.0F;
    constexpr float StartWidth = 68.0F;
    drawRoundedRect(StartX, PillY, StartWidth, PillHeight, 8.0F, 0.10F, C2D_Color32(58, 58, 58, 255));
    ui().drawCentered("START", StartX + StartWidth * 0.5F, PillY + 3.0F, 0.32F, C2D_Color32(240, 240, 240, 255));
}

void BankScreen::renderLocalBoxHeader() {
    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 6.0F, 26.0F, 0.14F);
    } else {
        drawPill(6.0F, 26.0F, 200.0F, 26.0F, 0.14F, BoxPlateBorder);
        drawPill(8.0F, 27.0F, 196.0F, 23.0F, 0.15F, BoxPlate);
    }
    const std::string label = session_.localBoxName.empty()
        ? format(TextId::LocalBoxLabel, {std::to_string(session_.localBox + 1)})
        : session_.localBoxName;
    ui().drawCentered(label, 106.0F, 31.0F, 0.55F, HeaderInk);
}

void BankScreen::renderLocalGrid() {
    const BankLayout::BoxGrid grid = BankLayout::localGrid(session_.grid(StoragePane::Local));
    drawGridCorners(grid);
    const StorageAddress address{StoragePane::Local, false};
    for (std::size_t slot = 0; slot < grid.slotCount(); ++slot) {
        const float cx = grid.centerX(slot);
        const float cy = grid.centerY(slot);
        drawSprite(session_.local.summaries[slot], cx, cy, 0.3F, 0.36F, false);
        drawHeldRegion(address, slot, cx, cy, grid.pitchX * 0.5F, grid.pitchY * 0.5F);
        if (focused(StoragePane::Local, slot)) {
            drawFocusCursor(cx, cy, 20.0F, 3.0F, 10.0F);
        }
    }
    drawMarkedCells(StoragePane::Local);
}

void BankScreen::renderTeamHeader() {
    constexpr float X = 218.0F;
    constexpr float Y = 26.0F;
    constexpr float Width = 96.0F;
    constexpr float Height = 26.0F;
    const bool themed = app_.resources_.teamBackground != nullptr;
    if (themed) {
        drawStretched(app_.resources_.teamBackground, X, Y, 0.14F, 320.0F - X, Height);
    } else {
        drawPill(X, Y, Width, Height, 0.14F, BoxPlateBorder);
        drawPill(X + 2.0F, Y + 1.0F, Width - 4.0F, Height - 3.0F, 0.15F, BoxPlate);
    }
    ui().drawCentered(text(TextId::TeamLabel), X + Width * 0.5F, 31.0F, 0.55F, themed ? White : HeaderInk);
}

void BankScreen::renderPartyGrid() {
    constexpr float Tile = BankLayout::PartyTileSize;
    const StorageAddress address{StoragePane::Party, false};
    const bool lastMemberLocked = session_.party.occupiedCount() <= 1;
    for (std::size_t slot = 0; slot < PartySlotCount; ++slot) {
        const auto [cx, cy] = BankLayout::partyTileCenter(slot);
        drawRoundedRect(cx - Tile * 0.5F, cy - Tile * 0.5F, Tile, Tile, 8.0F, 0.10F, CursorGreen);
        drawRoundedRect(cx - Tile * 0.5F + 2.0F, cy - Tile * 0.5F + 2.0F, Tile - 4.0F, Tile - 4.0F, 7.0F, 0.11F,
                        BoxPlate);
        drawSprite(session_.party.summaries[slot], cx, cy, 0.3F, 0.36F, lastMemberLocked);
        drawHeldRegion(address, slot, cx, cy, Tile * 0.5F, Tile * 0.5F);
        if (focused(StoragePane::Party, slot)) {
            drawFocusCursor(cx, cy, 20.0F, 3.0F, 10.0F);
        }
    }
    drawMarkedCells(StoragePane::Party);
}

void BankScreen::renderCommitOverlay() {
    const int progress = commit_.progress();
    C2D_DrawRectSolid(0.0F, 205.0F, 0.7F, 200.0F, 35.0F, C2D_Color32(0, 0, 0, 170));
    ui().drawCentered(std::string(text(commitPhaseLabel(commit_.phase()))) + " " + std::to_string(progress) + "%",
                      100.0F, 210.0F, 0.42F, White);
    C2D_DrawRectSolid(10.0F, 228.0F, 0.9F, 180.0F, 6.0F, C2D_Color32(50, 50, 50, 220));
    C2D_DrawRectSolid(10.0F, 228.0F, 0.92F, 180.0F * static_cast<float>(progress) / 100.0F, 6.0F, CursorGreen);
}

void BankScreen::renderActionHints() {
    const bool held = session_.hand.active;
    const bool pending = storage_.hasPendingChanges();
    constexpr u8 FadeAlphas[] = {25, 45, 65};
    for (int band = 0; band < 3; ++band) {
        C2D_DrawRectSolid(0.0F, 210.0F + band * 6.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, FadeAlphas[band]));
    }
    C2D_DrawRectSolid(0.0F, 228.0F, 0.06F, 320.0F, 12.0F, C2D_Color32(255, 255, 255, 85));

    const u32 divider = C2D_Color32(20, 110, 70, 55);
    C2D_DrawRectSolid(72.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);
    C2D_DrawRectSolid(152.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);

    const u32 idleGlyph = C2D_Color32(20, 110, 70, 140);
    const auto hint = [&](std::string_view glyph, float x, u32 glyphColor, TextId label, u32 labelColor) {
        ui().drawCentered(glyph, x, 219.0F, 0.42F, glyphColor);
        ui().drawText(text(label), x + 10.0F, 220.0F, 0.4F, labelColor);
    };
    hint("A", 16.0F, held ? CursorGreen : C2D_Color32(216, 40, 32, 150), held ? TextId::HintDrop : TextId::HintPick,
         HeaderInk);
    hint("B", 82.0F, held ? C2D_Color32(120, 60, 160, 220) : idleGlyph, held ? TextId::HintReturn : TextId::Back,
         HeaderInk);
    hint("S", 162.0F, pending ? CursorGreen : idleGlyph, TextId::HintSave, pending ? CursorGreen : HeaderInk);
}

void BankScreen::renderTrashConfirmDialog() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, DialogBackdrop);
    C2D_DrawRectSolid(18.0F, 52.0F, 0.40F, 284.0F, 148.0F, DialogPaper);
    ui().drawCentered(text(TextId::TrashCan), 160.0F, 68.0F, 0.5F, HeaderInk);
    ui().drawCentered(text(TextId::TrashConfirmMessage), 160.0F, 98.0F, 0.42F, HeaderInk);

    const auto button = [&](const UiRect& rect, u32 fill, TextId label) {
        C2D_DrawRectSolid(rect.x, rect.y, 0.46F, rect.width, rect.height, fill);
        ui().drawCentered(text(label), rect.x + rect.width * 0.5F, rect.y + 9.0F, 0.5F, White);
    };
    button(BankLayout::TrashYesButton, CursorGreen, TextId::Yes);
    button(BankLayout::TrashNoButton, Brand, TextId::No);
    ui().drawCentered("A / B", 160.0F, 180.0F, 0.36F, HeaderInk);
}
