#include "gui/screens/bank/BankScreen.hpp"

#include "gui/widgets/BoxBackground.hpp"
#include "gui/widgets/Cursor.hpp"
#include "gui/widgets/Shapes.hpp"
#include "platform/Clock.hpp"
#include "platform/Logger.hpp"
#include "save/catalog/GameCatalog.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

using namespace Gui;
using BankLayout::CloudGrid;

namespace {
bool incompatibleWithSave(const PokemonSummary& pokemon, std::uint8_t saveGeneration) {
    const GameDescriptor* origin = findGame(pokemon.gameCode);
    const std::uint8_t format = pokemon.format != 0
        ? pokemon.format
        : (origin ? static_cast<std::uint8_t>(origin->format) : 0);
    return saveGeneration != 0 && format != 0 && format > saveGeneration;
}
}

BankScreen::BankScreen(AppContext& app)
    : Screen(app),
      context_{app_.cloudApi, app_.bankLoads, app_.account.session, app_.status, app_.errors, app_.text,
               [this]() { leaveBank(); },
               [this](std::string& value, std::string_view hint, std::size_t maxLength) {
                   app_.navigator.requestText(value, hint, false, maxLength);
               }},
      session_(app_.saveAdapter),
      storage_(context_, session_),
      hand_(context_, session_, storage_),
      commit_(context_, session_, storage_),
      selection_(context_, session_, storage_),
      cloudSync_(context_, session_, storage_, hand_, commit_, selection_),
      input_(context_, session_, storage_, hand_, cloudSync_, commit_, selection_),
      sprites_(app_.resources),
      infoPanel_(app_.ui, app_.resources, app_.text),
      hud_(app_.ui, app_.text) {}

void BankScreen::pollBackgroundWork() {
    commit_.poll();
    cloudSync_.pollRenameBox();
}

void BankScreen::leaveBank() {
    app_.navigator.show(ScreenId::GameSelect);
    app_.music.setActive(false);
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
    app_.status.clear();
    leaveBank();
    app_.navigator.showError(TextId::CartridgeRemovedTitle, TextId::CartridgeRemovedMessage);
}

void BankScreen::update(const InputFrame& input) {
    pollCartridgeSlot();
    if (app_.navigator.current() == ScreenId::Bank) {
        input_.handle(input);
    }
}

void BankScreen::onGameOpened() {
    storage_.initializeFromOpenedGame(app_.saveLoads.openGameResult, app_.cloudCache.firstBox,
                                      app_.cloudCache.boxNames);
    app_.navigator.show(ScreenId::Bank);
    const auto games = supportedGames();
    const std::size_t catalogIndex = app_.saveLoads.catalogIndex;
    app_.music.setActive(catalogIndex < games.size()
                          && games[catalogIndex].platform == GamePlatform::VirtualConsole);
}

bool BankScreen::focused(StoragePane pane, std::size_t slot) const {
    return session_.storagePane == pane && session_.focusedSlot == slot;
}

const PokemonSummary& BankScreen::focusedSummary() const {
    return storage_.slots().peek(session_.focusedAddress(), session_.focusedSlot);
}

BankHud::HandState BankScreen::handState() const {
    if (fetchingHandPayload()) {
        return BankHud::HandState::Fetching;
    }
    return session_.hand.active ? BankHud::HandState::Holding : BankHud::HandState::Ready;
}

bool BankScreen::fetchingHandPayload() const {
    const LoadService& loads = app_.bankLoads;
    const Hand& hand = session_.hand;
    const bool awaitingHand = hand.active && hand.source.isCloudBank() && !hand.payloadKnown
        && loads.operation() == LoadService::Operation::PickupCloud;
    return loads.running() && (awaitingHand || loads.operation() == LoadService::Operation::SwapCloud);
}

void BankScreen::drawHeldRegion(StorageAddress address, std::size_t slot, float cx, float cy, float halfWidth,
                                float halfHeight) const {
    const auto held = selection_.heldSummaryAt(address, slot);
    if (!held) {
        return;
    }
    C2D_DrawRectSolid(cx - halfWidth, cy - halfHeight, SelectionHighlightDepth, halfWidth * 2.0F, halfHeight * 2.0F,
                      SelectionHighlightFill);
    sprites_.drawCarried(*held, cx, cy, HeldRegionSpriteDepth);
}

void BankScreen::drawFocusCursor(float cx, float cy, float cursorYOffset, float radius, float height) const {
    const bool carrying = session_.hand.active || selection_.holding();
    drawBouncingCursor(cx, cy - cursorYOffset, radius, height, carrying ? CursorGreen : BankHud::modeAccent(selection_.mode()));
    if (session_.hand.active) {
        sprites_.drawCarried(session_.hand.summary, cx, cy, 0.6F);
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
    const double since = Clock::secondsSince(session_.trashTransitionStart);
    const float step = static_cast<float>(std::clamp(since / TransitionSeconds, 0.0, 1.0));
    drawBoxBackground(app_.resources.boxBackground, session_.trashBoxActive ? step : 1.0F - step);
    renderTopHeader();
    renderCloudGrid(eyeOffset);
    infoPanel_.render(focusedSummary(), session_.storagePane);
}

void BankScreen::renderTopHeader() {
    constexpr float BarWidth = 200.0F * BankLayout::TopScale;
    constexpr float BarY = 6.0F;
    if (app_.resources.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources.boxNameBarSheet, 0), 8.0F, BarY, 0.14F, nullptr,
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

    const LoadService& loads = app_.bankLoads;
    const bool loadingThisBox = loads.running() && loads.operation() == LoadService::Operation::CloudBox
        && loads.cloudBoxKey == session_.cloudKey();
    const bool waitingOnHeldRegion = selection_.holding() && selection_.nextPayloadRequest().has_value();
    if (loadingThisBox || fetchingHandPayload() || waitingOnHeldRegion) {
        const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(Clock::seconds()) * 6.0F);
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
        sprites_.draw(pokemon, cx, cy, 0.3F, 0.31F, incompatibleWithSave(pokemon, saveGeneration), eyeOffset * 0.2F);
        drawHeldRegion(address, slot, cx, cy, CloudGrid.pitchX * 0.5F, CloudGrid.pitchY * 0.5F);
        if (focused(StoragePane::Cloud, slot) && !session_.cloudNameFocused) {
            drawFocusCursor(cx, cy, 22.0F, 3.5F, 12.0F);
        }
    }
    drawMarkedCells(StoragePane::Cloud);
}

void BankScreen::render() {
    if (session_.trashConfirmVisible) {
        hud_.drawTrashConfirm();
        return;
    }
    drawLinePattern(app_.resources.bottomBackground, C2D_Color32(158, 224, 152, 255));
    hud_.drawStatusBar(selection_.mode(), handState(), storage_.hasPendingChanges());
    renderLocalBoxHeader();
    renderLocalGrid();
    renderTeamHeader();
    renderPartyGrid();
    if (commit_.running()) {
        hud_.drawCommitProgress(commit_.phase(), commit_.progress());
    } else {
        hud_.drawActionHints(session_.hand.active, storage_.hasPendingChanges());
    }
}

void BankScreen::renderLocalBoxHeader() {
    if (app_.resources.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources.boxNameBarSheet, 0), 6.0F, 26.0F, 0.14F);
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
        sprites_.draw(session_.local.summaries[slot], cx, cy, 0.3F, 0.36F, false);
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
    const bool themed = app_.resources.teamBackground != nullptr;
    if (themed) {
        Gui::drawStretched(app_.resources.teamBackground, X, Y, 0.14F, 320.0F - X, Height);
    } else {
        drawPill(X, Y, Width, Height, 0.14F, BoxPlateBorder);
        drawPill(X + 2.0F, Y + 1.0F, Width - 4.0F, Height - 3.0F, 0.15F, BoxPlate);
    }
    ui().drawCentered(text(TextId::TeamLabel), X + Width * 0.5F, 31.0F, 0.55F, themed ? Surface : HeaderInk);
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
        sprites_.draw(session_.party.summaries[slot], cx, cy, 0.3F, 0.36F, lastMemberLocked);
        drawHeldRegion(address, slot, cx, cy, Tile * 0.5F, Tile * 0.5F);
        if (focused(StoragePane::Party, slot)) {
            drawFocusCursor(cx, cy, 20.0F, 3.0F, 10.0F);
        }
    }
    drawMarkedCells(StoragePane::Party);
}
