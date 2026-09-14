#pragma once

#include "bank/BankInputController.hpp"
#include "bank/BankSession.hpp"
#include "bank/CloudSyncController.hpp"
#include "bank/CommitService.hpp"
#include "selection/SelectionController.hpp"
#include "bank/StorageController.hpp"
#include "save/adapter/SaveAdapter.hpp"

#include <citro2d.h>
#include <3ds.h>

#include <utility>

class App;

// Pure presentation for the bank screen. All storage, cloud-sync and commit
// logic lives in the bank/ package (BankSession + *Controller/*Service); this
// class only owns those collaborators, forwards input/lifecycle calls to
// them, and draws.
class BankScreen {
public:
    explicit BankScreen(App& app)
        : app_(app),
          storage_(app_, session_),
          commit_(app_, session_, storage_),
          selection_(app_, session_, storage_),
          cloudSync_(app_, session_, commit_, selection_),
          input_(app_, session_, storage_, cloudSync_, commit_, selection_) {}

    void update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);
    void renderTop(float eyeOffset);
    void render();

    void pollCommit() { commit_.poll(); }
    void pollRenameBox() { cloudSync_.pollRenameBox(); }

    void onGameOpened();
    void onCloudBoxLoaded() { cloudSync_.onCloudBoxLoaded(); }
    void onCloudPickupCompleted() { cloudSync_.onCloudPickupCompleted(); }
    void onCloudSwapCompleted() { cloudSync_.onCloudSwapCompleted(); }

    void reset() { storage_.reset(); }

    bool errorDialogVisible() const { return session_.errorDialogVisible; }
    void dismissErrorDialog() { session_.errorDialogVisible = false; }

    SaveAdapter& saveAdapter() { return session_.saveAdapter; }

private:
    void pollCartridgeSlot();
    void renderStorageBottom();
    void renderStatusBar();
    void renderLocalBoxHeader();
    void renderLocalGrid();
    void renderMarkedArea(float pitchX, float pitchY, float gridLeft, float gridTop, StoragePane pane);
    void renderMarkedPartyArea();
    void drawSelectionOverlay(float cx, float cy, float halfWidth, float halfHeight) const;
    std::pair<float, float> partyTileCenter(std::size_t slot) const;
    void renderTeamHeader();
    void renderPartyGrid();
    void renderCommitOverlay();
    void renderActionHints();
    void renderErrorDialog();
    void renderTrashConfirmDialog();
    void renderTopHeader();
    void renderTopBoxGrid(float eyeOffset);
    void renderTopInfoPanel();
    void drawCarriedSprite(const PokemonSummary& summary, float cx, float cy, float z) const;
    void drawHeldPokemonPreview(float cx, float cy) const;
    void drawHeldRegionSprite(const PokemonSummary& summary, float cx, float cy) const;
    void drawFocusCursor(float cx, float cy, float cursorYOffset, float radius, float height) const;
    u32 focusCursorColor() const;
    u32 selectionModeAccent() const;

    App& app_;
    BankSession session_;
    StorageController storage_;
    CommitService commit_;
    SelectionController selection_;
    CloudSyncController cloudSync_;
    BankInputController input_;
    bool cardInsertionKnown_ = false;
    bool cardInserted_ = false;
};
