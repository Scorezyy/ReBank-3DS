#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankInputController.hpp"
#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/StorageController.hpp"
#include "bank/commit/CommitService.hpp"
#include "bank/selection/SelectionController.hpp"
#include "gui/Screen.hpp"
#include "gui/screens/bank/BankHud.hpp"
#include "gui/screens/bank/BankLayout.hpp"
#include "gui/screens/bank/PokemonInfoPanel.hpp"
#include "gui/screens/bank/PokemonSprites.hpp"
#include "save/storage/CartridgeSlotMonitor.hpp"

#include <citro2d.h>

#include <optional>
#include <string_view>

class BankScreen : public Screen {
public:
    explicit BankScreen(AppContext& app);

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
    void pollCartridgeSlot();
    void leaveBank();

    void renderTopHeader();
    void renderCloudGrid(float eyeOffset);

    void renderLocalBoxHeader();
    void renderLocalGrid();
    void renderTeamHeader();
    void renderPartyGrid();

    void drawGridCorners(const BankLayout::BoxGrid& grid) const;
    void drawMarkedCells(StoragePane pane) const;
    void drawHeldRegion(StorageAddress address, std::size_t slot, float cx, float cy, float halfWidth,
                        float halfHeight) const;
    void drawFocusCursor(float cx, float cy, float cursorYOffset, float radius, float height) const;
    bool focused(StoragePane pane, std::size_t slot) const;
    const PokemonSummary& focusedSummary() const;
    bool fetchingHandPayload() const;
    BankHud::HandState handState() const;

    BankContext context_;
    BankSession session_;
    StorageController storage_;
    HandController hand_;
    CommitService commit_;
    SelectionController selection_;
    CloudSyncController cloudSync_;
    BankInputController input_;
    CartridgeSlotMonitor cartridgeSlot_;
    PokemonSprites sprites_;
    PokemonInfoPanel infoPanel_;
    BankHud hud_;
};
