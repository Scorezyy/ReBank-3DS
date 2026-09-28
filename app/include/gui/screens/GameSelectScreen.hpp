#pragma once

#include "gui/Screen.hpp"
#include "io/CartridgeSlotMonitor.hpp"
#include "save/SaveLoadService.hpp"

#include <citro2d.h>

#include <cstddef>
#include <string_view>
#include <vector>

class GameSelectScreen : public Screen {
public:
    using Screen::Screen;
    ~GameSelectScreen() override { reset(); }

    void update(const InputFrame& input) override;
    void renderTop(float eyeOffset) override;
    void render() override;

    void refresh();
    void populateFromDiscovered(std::vector<DiscoveredGame>& discovered);
    void applySummary(std::size_t catalogIndex, bool cartridge, const SaveSummary& summary);
    void reset();

private:
    struct GameProfile : GameSource {
        bool cartridgeEmpty = false;
        bool summaryPending = false;
        C3D_Tex iconTexture{};
        Tex3DS_SubTexture iconSubTexture{};
        bool iconLoaded = false;
    };

    static void uploadIcon(GameProfile& profile, const IconPixels& pixels);
    void select(std::size_t index, int direction);
    void openSelected();
    void requestNextSummary();
    void pollCartridgeSlot();
    void drawIcon(const GameProfile& profile, float centerX, float centerY, float size, float z);
    void drawHintChip(float x, float y, std::string_view key, std::string_view label);
    void drawSaveDetails(const GameProfile& profile);
    float carouselEase() const;

    std::size_t index_ = 0;
    std::vector<GameProfile> games_;
    u64 selectionChangedAt_ = 0;
    int selectionDirection_ = 0;
    CartridgeSlotMonitor cartridgeSlot_;
    bool openRequested_ = false;
};
