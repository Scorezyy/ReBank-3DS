#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankInputController.hpp"
#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/CommitService.hpp"
#include "bank/StorageController.hpp"
#include "gui/BankLayout.hpp"
#include "gui/Screen.hpp"
#include "io/CartridgeSlotMonitor.hpp"
#include "selection/SelectionController.hpp"

#include <citro2d.h>

#include <optional>
#include <string_view>

class BankScreen : public Screen {
public:
    explicit BankScreen(App& app);

    void update(const InputFrame& input) override;
    void renderTop(float eyeOffset) override;
    void render() override;

    void pollBackgroundWork();
    bool backgroundWorkRunning() const { return commit_.running() || cloudSync_.renameInProgress(); }

    void onGameOpened();
    void onCloudBoxLoaded() { cloudSync_.onCloudBoxLoaded(); }
    void onCloudPickupCompleted() { cloudSync_.onCloudPickupCompleted(); }
    void onCloudSwapCompleted() { cloudSync_.onCloudSwapCompleted(); }
    void reset() { storage_.reset(); }

private:
    struct InfoRow {
        float y;
        int index;
    };

    void pollCartridgeSlot();
    void leaveBank();

    void renderTopHeader();
    void renderCloudGrid(float eyeOffset);
    void renderInfoPanel();
    void drawInfoHeader(const PokemonSummary& focused, const std::string& speciesName);
    void drawInfoStripe(const InfoRow& row) const;
    void drawInfoRow(InfoRow& row, std::string_view label, std::string_view value, float fontSize);
    void drawTypeRow(InfoRow& row, const PokemonSummary& focused);

    void renderStatusBar();
    void renderLocalBoxHeader();
    void renderLocalGrid();
    void renderTeamHeader();
    void renderPartyGrid();
    void renderCommitOverlay();
    void renderActionHints();
    void renderTrashConfirmDialog();

    void drawGridCorners(const BankLayout::BoxGrid& grid) const;
    void drawMarkedCells(StoragePane pane) const;
    std::optional<C2D_Image> spriteOf(const PokemonSummary& summary) const;
    void drawSprite(const PokemonSummary& summary, float cx, float cy, float z, float badgeZ, bool dimmed,
                    float xShift = 0.0F) const;
    void drawCarriedSprite(const PokemonSummary& summary, float cx, float cy, float z) const;
    void drawHeldRegion(StorageAddress address, std::size_t slot, float cx, float cy, float halfWidth,
                        float halfHeight) const;
    void drawFocusCursor(float cx, float cy, float cursorYOffset, float radius, float height) const;
    bool focused(StoragePane pane, std::size_t slot) const;
    const PokemonSummary& focusedSummary() const;
    bool fetchingHandPayload() const;
    u32 selectionModeAccent() const;

    BankContext context_;
    BankSession session_;
    StorageController storage_;
    CommitService commit_;
    SelectionController selection_;
    CloudSyncController cloudSync_;
    BankInputController input_;
    CartridgeSlotMonitor cartridgeSlot_;
};
