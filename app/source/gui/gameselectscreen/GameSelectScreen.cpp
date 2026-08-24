#include "gui/gameselectscreen/GameSelectScreen.hpp"
#include "app/App.hpp"
#include "BuildConfig.hpp"
#include "core/Logger.hpp"
#include "gui/GameVisual.hpp"
#include "gui/elements/TextMetrics.hpp"
#include "gui/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <new>

using namespace Gui;

void GameSelectScreen::update(u32 keysDown, touchPosition touch, bool touched) {
    if (touched && LogoutButton.contains(touch)) {
        app_.logout();
        return;
    }
    if (keysDown & KEY_X) {
        app_.status_ = "Finding save games...";
        refresh();
        app_.status_ = games_.empty() ? "No compatible save game found." : std::string{};
        return;
    }
    if (games_.empty()) {
        return;
    }
    if (keysDown & (KEY_LEFT | KEY_L)) {
        const std::size_t next = index_ == 0 ? games_.size() - 1 : index_ - 1;
        select(next, -1);
    }
    if (keysDown & (KEY_RIGHT | KEY_R)) {
        select((index_ + 1) % games_.size(), 1);
    }
    if ((keysDown & KEY_A) || (touched && UiRect{48.0F, 56.0F, 224.0F, 128.0F}.contains(touch))) {
        openSelected();
        return;
    }
}

void GameSelectScreen::refresh() {
    app_.status_ = "Finding save games...";
    app_.loadService_.begin(LoadService::Operation::DiscoverGames);
}

void GameSelectScreen::select(std::size_t index, int direction) {
    if (index >= games_.size() || index == index_) {
        return;
    }
    index_ = index;
    selectionDirection_ = direction;
    selectionChangedAt_ = svcGetSystemTick();
}

bool GameSelectScreen::openSelected() {
    if (index_ >= games_.size()) {
        return false;
    }
    const auto games = supportedGames();
    const GameProfile& profile = games_[index_];
    if (profile.catalogIndex >= games.size()) {
        return false;
    }
    const GameDescriptor& game = games[profile.catalogIndex];
    Logger::instance().info("Game selected: " + std::string(game.code));
    app_.status_ = "Reading save...";
    app_.loadService_.catalogIndex = profile.catalogIndex;
    app_.loadService_.begin(LoadService::Operation::OpenGame);
    return app_.loadService_.running();
}

void GameSelectScreen::reset() {
    for (auto& profile : games_) {
        if (profile.iconLoaded) {
            C3D_TexDelete(&profile.iconTexture);
        }
    }
    games_.clear();
}

void GameSelectScreen::populateFromDiscovered(std::vector<DiscoveredGame>& discovered) {
    reset();
    std::stable_partition(discovered.begin(), discovered.end(),
        [](const DiscoveredGame& game) { return game.cartridge; });
    games_.reserve(discovered.size());
    for (DiscoveredGame& game : discovered) {
        GameProfile profile;
        profile.catalogIndex = game.catalogIndex;
        profile.save = std::move(game.save);
        profile.cartridge = game.cartridge;
        games_.push_back(std::move(profile));
        GameProfile& stored = games_.back();
        if (!game.iconPixels
            || !C3D_TexInit(&stored.iconTexture, 64, 64, GPU_RGB565)) {
            continue;
        }
        const std::unique_ptr<std::array<std::uint16_t, 64 * 64>> tiled(
            new (std::nothrow) std::array<std::uint16_t, 64 * 64>());
        if (!tiled) {
            C3D_TexDelete(&stored.iconTexture);
            continue;
        }
        for (std::size_t y = 0; y < 48; ++y) {
            for (std::size_t x = 0; x < 48; ++x) {
                const std::size_t pixel = ((x & 1) | ((y & 1) << 1)
                    | ((x & 2) << 1) | ((y & 2) << 2)
                    | ((x & 4) << 2) | ((y & 4) << 3));
                const std::size_t destination = ((y / 8) * 8 + x / 8) * 64 + pixel;
                (*tiled)[destination] = (*game.iconPixels)[y * 48 + x];
            }
        }
        C3D_TexUpload(&stored.iconTexture, tiled->data());
        C3D_TexSetFilter(&stored.iconTexture, GPU_LINEAR, GPU_LINEAR);
        stored.iconSubTexture = {48, 48, 0.0F, 1.0F, 0.75F, 0.25F};
        stored.iconLoaded = true;
    }
    Logger::instance().info("All game icon textures built");
    discovered.clear();
    index_ = 0;
    selectionChangedAt_ = svcGetSystemTick();
    selectionDirection_ = 0;
    app_.status_ = games_.empty() ? "No compatible save game found." : std::string{};
    app_.screen_ = App::Screen::GameSelect;
    Logger::instance().info("Detected " + std::to_string(games_.size()) + " save games");
}

void GameSelectScreen::drawIcon(const GameProfile& profile, float centerX, float centerY, float size, float z) {
    const GameDescriptor& game = supportedGames()[profile.catalogIndex];
    const GameVisual visual = gameVisual(game.code);
    const float x = centerX - size * 0.5F;
    const float y = centerY - size * 0.5F;
    C2D_DrawRectSolid(x - 4.0F, y - 4.0F, z, size + 8.0F, size + 8.0F, Ink);
    if (profile.iconLoaded) {
        const C2D_Image image{const_cast<C3D_Tex*>(&profile.iconTexture), &profile.iconSubTexture};
        C2D_DrawImageAt(image, x, y, z + 0.01F, nullptr, size / 48.0F, size / 48.0F);
        return;
    }
    C2D_DrawRectSolid(x, y, z + 0.01F, size, size, visual.primary);
    C2D_DrawRectSolid(x + 6.0F, y + 6.0F, z + 0.02F, size - 12.0F, size - 12.0F, visual.secondary);
    C2D_DrawCircleSolid(centerX, centerY, z + 0.03F, size * 0.25F, Surface);
    C2D_DrawRectSolid(centerX - size * 0.25F, centerY - 2.0F, z + 0.04F,
                      size * 0.5F, 4.0F, Ink);
}

void GameSelectScreen::renderTop(float eyeOffset) {
    const std::string buildLabel = BuildConfig::label();
    const float buildSize = 0.30F;
    app_.drawText(buildLabel, 394.0F - textWidth(app_.resources_.textFont, app_.resources_.textBuffer, buildLabel, buildSize),
             225.0F, buildSize, Muted);
    if (games_.empty()) {
        app_.drawCentered("No compatible save game", 200.0F, 92.0F, 0.78F, Error);
        app_.drawCentered("Insert a cartridge or create a save first.", 200.0F, 130.0F, 0.48F, Muted);
        return;
    }
    const auto games = supportedGames();
    const GameProfile& profile = games_[index_];
    const GameDescriptor& game = games[profile.catalogIndex];
    const double elapsed = static_cast<double>(svcGetSystemTick() - selectionChangedAt_)
        / SYSCLOCK_ARM11;
    const float progress = std::min(1.0F, static_cast<float>(elapsed) * 5.0F);
    const float eased = 1.0F - (1.0F - progress) * (1.0F - progress);
    const float slide = static_cast<float>(selectionDirection_) * (1.0F - eased) * 90.0F;

    for (float x = 0.0F; x < 400.0F; x += 20.0F) {
        C2D_DrawRectSolid(x, 0.0F, 0.01F, 1.0F, 240.0F, C2D_Color32(105, 180, 116, 35));
    }
    drawIcon(profile, 200.0F + eyeOffset + slide, 54.0F, 62.0F, 0.12F);
    if (profile.cartridge) {
        C2D_DrawRectSolid(252.0F + slide, 22.0F, 0.2F, 88.0F, 18.0F, CursorGreen);
        app_.drawCentered("CARTRIDGE", 296.0F + slide, 26.0F, 0.34F, Surface);
    }
    C2D_DrawRectSolid(0.0F, 91.0F, 0.1F, 400.0F, 33.0F, gameVisual(game.code).primary);
    app_.drawCentered(game.name, 200.0F + slide, 99.0F, 0.68F, Surface);

    app_.drawText(profile.save.trainerName.empty() ? "Unknown Trainer" : profile.save.trainerName,
             28.0F, 144.0F, 0.56F, Ink);
    app_.drawText("ID No. " + paddedTrainerId(profile.save.trainerId), 222.0F, 144.0F, 0.56F, Ink);
    const std::uint32_t hours = profile.save.playTimeMinutes / 60;
    const std::uint32_t minutes = profile.save.playTimeMinutes % 60;
    app_.drawText("Play time: " + std::to_string(hours) + ":"
             + (minutes < 10 ? "0" : "") + std::to_string(minutes),
             28.0F, 188.0F, 0.54F, Ink);
    app_.drawText("Pokedex: " + std::to_string(profile.save.pokedexCount),
             222.0F, 188.0F, 0.54F, Ink);
}

void GameSelectScreen::render() {
    app_.drawCentered("Choose the Pokemon title to use", 160.0F, 18.0F, 0.62F, Ink);
    app_.drawButton(LogoutButton, "Logout", false);
    if (games_.empty()) {
        app_.drawCentered(app_.status_, 160.0F, 106.0F, 0.44F, Error);
        return;
    }
    const auto games = supportedGames();
    const std::size_t count = games_.size();
    const std::size_t previous = index_ == 0 ? count - 1 : index_ - 1;
    const std::size_t next = (index_ + 1) % count;
    if (count > 1) {
        drawIcon(games_[previous], 56.0F, 94.0F, 42.0F, 0.1F);
        drawIcon(games_[next], 264.0F, 94.0F, 42.0F, 0.1F);
        app_.drawText("<", 18.0F, 86.0F, 0.72F, Muted);
        app_.drawText(">", 294.0F, 86.0F, 0.72F, Muted);
    }
    const GameDescriptor& selected = games[games_[index_].catalogIndex];
    drawIcon(games_[index_], 160.0F, 104.0F, 82.0F, 0.2F);
    app_.drawCentered(selected.name, 160.0F, 157.0F, 0.54F, Ink);
    app_.drawCentered(std::to_string(index_ + 1) + " / " + std::to_string(count),
                 160.0F, 181.0F, 0.38F, Muted);
    app_.drawCentered("A Select   L/R Browse   X Rescan", 205.0F, 211.0F, 0.31F, Muted);
}
