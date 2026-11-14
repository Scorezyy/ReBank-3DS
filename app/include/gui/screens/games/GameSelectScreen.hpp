#pragma once

#include "gui/Screen.hpp"
#include "gui/icons/IconTexture.hpp"
#include "save/SaveLoadService.hpp"
#include "save/storage/CartridgeSlotMonitor.hpp"

#include <citro2d.h>

#include <cstddef>
#include <string_view>
#include <memory>
#include <vector>

class GameSelectScreen : public Screen {
public:
    using Screen::Screen;
    ~GameSelectScreen() override { reset(); }

    void update(const InputFrame& input) override;
    void renderTop(float eyeOffset) override;
    void render() override;

    void refresh(bool rebuildLibrary);
    void populateFromDiscovered(std::vector<DiscoveredGame>& discovered);
    void applySummary(std::size_t catalogIndex, bool cartridge, const SaveSummary& summary);
    void reset();

private:
    struct GameProfile : GameSource {
        bool cartridgeEmpty = false;
        bool summaryPending = false;
        std::unique_ptr<IconTexture> icon;
    };

    void select(std::size_t index, int direction);
    void openSelected();
    void requestNextSummary();
    void pollCartridgeSlot();
    void drawIcon(const GameProfile& profile, float centerX, float centerY, float size, float z);
    void drawHintChip(float x, float y, std::string_view key, std::string_view label);
    void drawSaveDetails(const GameProfile& profile);
    void drawSideButtons();
    void drawCarousel();
    void drawPageIndicator();
    float carouselEase() const;

    std::size_t index_ = 0;
    std::vector<GameProfile> games_;
    u64 selectionChangedAt_ = 0;
    int selectionDirection_ = 0;
    CartridgeSlotMonitor cartridgeSlot_;
    bool openRequested_ = false;
};
