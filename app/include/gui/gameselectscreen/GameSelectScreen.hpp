#pragma once

#include "gui/Theme.hpp"
#include "network/LoadService.hpp"
#include "save/SaveAdapter.hpp"

#include <citro2d.h>
#include <3ds.h>

#include <cstddef>
#include <string_view>
#include <vector>

class App;

// Lets the player browse detected Pokemon saves and pick one to open.
class GameSelectScreen {
public:
    explicit GameSelectScreen(App& app) : app_(app) {}

    void update(u32 keysDown, touchPosition touch, bool touched);
    void renderTop(float eyeOffset);
    void render();

    void refresh();
    void populateFromDiscovered(std::vector<DiscoveredGame>& discovered);
    void reset();

private:
    struct GameProfile {
        std::size_t catalogIndex = 0;
        SaveSummary save;
        bool cartridge = false;
        C3D_Tex iconTexture{};
        Tex3DS_SubTexture iconSubTexture{};
        bool iconLoaded = false;
    };

    void select(std::size_t index, int direction);
    bool openSelected();
    void drawIcon(const GameProfile& profile, float centerX, float centerY, float size, float z,
                  u32 borderColor = Gui::Ink);
    void drawHintChip(float x, float y, std::string_view key, std::string_view label);
    float carouselEase() const;

    App& app_;
    std::size_t index_ = 0;
    std::vector<GameProfile> games_;
    u64 selectionChangedAt_ = 0;
    int selectionDirection_ = 0;
};
