#include "gui/screens/GameSelectScreen.hpp"
#include "app/App.hpp"
#include "BuildConfig.hpp"
#include "core/Logger.hpp"
#include "gui/GameVisual.hpp"
#include "gui/elements/Shapes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <new>

using namespace Gui;

namespace {
constexpr UiRect SelectedCard{48.0F, 56.0F, 224.0F, 128.0F};
constexpr std::size_t IconTextureSize = 64;

double secondsNow() {
    return static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
}

std::string paddedTrainerId(std::uint32_t trainerId) {
    std::string value = std::to_string(trainerId);
    return value.size() >= 5 ? value : std::string(5 - value.size(), '0') + value;
}

std::string playTime(std::uint32_t minutes) {
    const std::uint32_t remainder = minutes % 60;
    return std::to_string(minutes / 60) + ":" + (remainder < 10 ? "0" : "") + std::to_string(remainder);
}

std::string loadingDots() {
    return std::string(static_cast<std::size_t>(1 + static_cast<int>(secondsNow() * 2.0) % 3), '.');
}

void drawMissingIcon(float centerX, float centerY, float size, float z) {
    C2D_DrawCircleSolid(centerX, centerY, z + 0.03F, size * 0.25F, Surface);
    C2D_DrawRectSolid(centerX - size * 0.25F, centerY - 2.0F, z + 0.04F, size * 0.5F, 4.0F, Ink);
}

std::size_t previousIndex(std::size_t index, std::size_t count) {
    return index == 0 ? count - 1 : index - 1;
}
}

void GameSelectScreen::pollCartridgeSlot() {
    if (app_.saveLoadService_.running()) {
        return;
    }
    switch (cartridgeSlot_.poll()) {
        case CartridgeSlotMonitor::Change::Removed:
            app_.saveLoadService_.dropCartridgeGames();
            populateFromDiscovered(app_.saveLoadService_.discoveredGames);
            return;
        case CartridgeSlotMonitor::Change::Inserted:
            app_.status_ = text(TextId::StatusCheckingCartridgeSlot);
            app_.saveLoadService_.begin(SaveLoadService::Operation::RescanCartridge);
            return;
        case CartridgeSlotMonitor::Change::None:
            return;
    }
}

void GameSelectScreen::update(const InputFrame& input) {
    pollCartridgeSlot();
    if (openRequested_ && !app_.saveLoadService_.busy()) {
        openRequested_ = false;
        openSelected();
        return;
    }
    requestNextSummary();
    if (input.tapped(LogoutButton)) {
        app_.logout();
        return;
    }
    if (input.pressed(KEY_X)) {
        refresh();
        return;
    }
    if (games_.empty()) {
        return;
    }
    if (input.pressed(KEY_LEFT | KEY_L)) {
        select(previousIndex(index_, games_.size()), -1);
    }
    if (input.pressed(KEY_RIGHT | KEY_R)) {
        select((index_ + 1) % games_.size(), 1);
    }
    if (input.pressed(KEY_A) || input.tapped(SelectedCard)) {
        openSelected();
    }
}

void GameSelectScreen::refresh() {
    app_.status_ = text(TextId::StatusFindingSaveGames);
    app_.saveLoadService_.begin(SaveLoadService::Operation::DiscoverGames);
}

void GameSelectScreen::select(std::size_t index, int direction) {
    if (index >= games_.size() || index == index_) {
        return;
    }
    index_ = index;
    selectionDirection_ = direction;
    selectionChangedAt_ = svcGetSystemTick();
}

void GameSelectScreen::openSelected() {
    if (index_ >= games_.size() || games_[index_].cartridgeEmpty) {
        return;
    }
    const GameProfile& profile = games_[index_];
    if (app_.saveLoadService_.busy()) {
        openRequested_ = true;
        app_.status_ = text(TextId::StatusWaitingForLoader);
        return;
    }
    Logger::instance().info("Game selected: " + std::string(supportedGames()[profile.catalogIndex].code));
    app_.status_ = text(TextId::StatusReadingSave);
    const SaveAdapter::SourcePreference preference = profile.cartridge
        ? SaveAdapter::SourcePreference::CartridgeOnly
        : (profile.storageOnly ? SaveAdapter::SourcePreference::StorageOnly : SaveAdapter::SourcePreference::Any);
    app_.saveLoadService_.beginOpen(profile.catalogIndex, preference);
}

void GameSelectScreen::reset() {
    openRequested_ = false;
    for (GameProfile& profile : games_) {
        if (profile.iconLoaded) {
            C3D_TexDelete(&profile.iconTexture);
        }
    }
    games_.clear();
}

void GameSelectScreen::uploadIcon(GameProfile& profile, const IconPixels& pixels) {
    const std::unique_ptr<std::array<std::uint16_t, IconTextureSize * IconTextureSize>> tiled(
        new (std::nothrow) std::array<std::uint16_t, IconTextureSize * IconTextureSize>());
    if (!tiled || !C3D_TexInit(&profile.iconTexture, IconTextureSize, IconTextureSize, GPU_RGB565)) {
        return;
    }
    for (std::size_t y = 0; y < GameIconSize; ++y) {
        for (std::size_t x = 0; x < GameIconSize; ++x) {
            const std::size_t pixel = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2)
                | ((x & 4) << 2) | ((y & 4) << 3);
            (*tiled)[((y / 8) * 8 + x / 8) * 64 + pixel] = pixels[y * GameIconSize + x];
        }
    }
    C3D_TexUpload(&profile.iconTexture, tiled->data());
    C3D_TexSetFilter(&profile.iconTexture, GPU_LINEAR, GPU_LINEAR);
    constexpr auto Edge = static_cast<u16>(GameIconSize);
    profile.iconSubTexture = {Edge, Edge, 0.0F, 1.0F, 0.75F, 0.25F};
    profile.iconLoaded = true;
}

void GameSelectScreen::populateFromDiscovered(std::vector<DiscoveredGame>& discovered) {
    reset();
    games_.reserve(discovered.size() + 1);
    std::stable_partition(discovered.begin(), discovered.end(),
        [](const DiscoveredGame& game) { return game.cartridge; });
    if (discovered.empty() || !discovered.front().cartridge) {
        GameProfile placeholder;
        placeholder.cartridge = true;
        placeholder.cartridgeEmpty = true;
        games_.push_back(placeholder);
    }
    for (DiscoveredGame& game : discovered) {
        GameProfile& profile = games_.emplace_back();
        static_cast<GameSource&>(profile) = std::move(game);
        profile.summaryPending = !game.summaryKnown || (game.cartridge && profile.save.trainerName.empty());
        if (game.iconPixels) {
            uploadIcon(profile, *game.iconPixels);
        }
    }
    Logger::instance().info("All game icon textures built");
    discovered.clear();
    index_ = 0;
    selectionChangedAt_ = svcGetSystemTick();
    selectionDirection_ = 0;
    app_.status_ = games_.empty() ? std::string(text(TextId::StatusNoCompatibleSaveGame)) : std::string{};
    app_.screen_ = ScreenId::GameSelect;
    Logger::instance().info("Detected " + std::to_string(games_.size()) + " save games");
}

void GameSelectScreen::requestNextSummary() {
    if (openRequested_ || app_.saveLoadService_.busy()) {
        return;
    }
    const auto pending = std::find_if(games_.begin(), games_.end(),
        [](const GameProfile& profile) { return !profile.cartridgeEmpty && profile.summaryPending; });
    if (pending != games_.end()) {
        app_.saveLoadService_.beginSummary(pending->catalogIndex, pending->readsCartridge());
    }
}

void GameSelectScreen::applySummary(std::size_t catalogIndex, bool cartridge, const SaveSummary& summary) {
    const auto match = std::find_if(games_.begin(), games_.end(), [&](const GameProfile& profile) {
        return profile.catalogIndex == catalogIndex && profile.summaryPending && profile.readsCartridge() == cartridge;
    });
    if (match != games_.end()) {
        match->save = summary;
        match->summaryPending = false;
    }
}

float GameSelectScreen::carouselEase() const {
    const double elapsed = static_cast<double>(svcGetSystemTick() - selectionChangedAt_) / SYSCLOCK_ARM11;
    const float progress = std::min(1.0F, static_cast<float>(elapsed) * 5.0F);
    return 1.0F - (1.0F - progress) * (1.0F - progress);
}

void GameSelectScreen::drawHintChip(float x, float y, std::string_view key, std::string_view label) {
    ui().drawText(key, x, y, 0.44F, Brand);
    ui().drawText(label, x + (key.size() > 1 ? 32.0F : 20.0F), y, 0.40F, Muted);
}

void GameSelectScreen::drawIcon(const GameProfile& profile, float centerX, float centerY, float size, float z) {
    const float x = centerX - size * 0.5F;
    const float y = centerY - size * 0.5F;
    const auto drawTexture = [&](float left, float top, float edge) {
        const C2D_Image image{const_cast<C3D_Tex*>(&profile.iconTexture), &profile.iconSubTexture};
        const float scale = edge / static_cast<float>(GameIconSize);
        C2D_DrawImageAt(image, left, top, z + 0.02F, nullptr, scale, scale);
    };
    if (profile.cartridge) {
        if (app_.resources_.gameSelectorCard) {
            const C2D_Image card = C2D_SpriteSheetGetImage(app_.resources_.gameSelectorCard, 0);
            const float scale = size / static_cast<float>(card.subtex->width);
            C2D_DrawImageAt(card, x, y, z + 0.01F, nullptr, scale, scale);
        }
        const float iconSize = size * 0.68F;
        if (profile.iconLoaded) {
            drawTexture(centerX - iconSize * 0.5F - size * 0.02F, centerY - iconSize * 0.5F + size * 0.01F, iconSize);
        } else if (!profile.cartridgeEmpty) {
            drawMissingIcon(centerX, centerY, iconSize, z);
        }
        return;
    }
    const GameVisual visual = gameVisual(supportedGames()[profile.catalogIndex].code);
    C2D_DrawRectSolid(x - 4.0F, y - 4.0F, z, size + 8.0F, size + 8.0F, Ink);
    C2D_DrawRectSolid(x, y, z + 0.01F, size, size, visual.primary);
    C2D_DrawRectSolid(x + 6.0F, y + 6.0F, z + 0.02F, size - 12.0F, size - 12.0F, visual.secondary);
    if (profile.iconLoaded) {
        drawTexture(x, y, size);
    } else {
        drawMissingIcon(centerX, centerY, size, z);
    }
}

void GameSelectScreen::drawSaveDetails(const GameProfile& profile) {
    const SaveSummary& save = profile.save;
    const std::string dots = profile.summaryPending ? loadingDots() : std::string{};
    const auto valueOr = [&](std::string value) { return profile.summaryPending ? dots : std::move(value); };
    const std::string trainer = profile.summaryPending || save.trainerName.empty()
        ? std::string(text(TextId::UnknownTrainer)) + dots
        : save.trainerName;
    ui().drawText(trainer, 28.0F, 144.0F, 0.56F, Ink);
    ui().drawText(std::string(text(TextId::IdNoPrefix)) + valueOr(paddedTrainerId(save.trainerId)),
                  222.0F, 144.0F, 0.56F, Ink);
    ui().drawText(std::string(text(TextId::PlayTimePrefix)) + valueOr(playTime(save.playTimeMinutes)),
                  28.0F, 188.0F, 0.54F, Ink);
    ui().drawText(std::string(text(TextId::PokedexPrefix)) + valueOr(std::to_string(save.pokedexCount)),
                  222.0F, 188.0F, 0.54F, Ink);
}

void GameSelectScreen::renderTop(float eyeOffset) {
    const std::string buildLabel = BuildConfig::label();
    constexpr float FooterSize = 0.30F;
    ui().drawText(buildLabel, 394.0F - ui().textWidth(buildLabel, FooterSize), 225.0F, FooterSize, Muted);
    ui().drawText("ID: " + app_.session_.accountId + " | \"" + app_.accountUsername_ + "\"", 6.0F, 225.0F,
                  FooterSize, Muted);
    if (games_.empty()) {
        ui().drawCentered(text(TextId::NoCompatibleSaveGameTitle), 200.0F, 92.0F, 0.78F, Error);
        ui().drawCentered(text(TextId::InsertCartridgeOrCreateSave), 200.0F, 130.0F, 0.48F, Muted);
        return;
    }
    const GameProfile& profile = games_[index_];
    const float slide = static_cast<float>(selectionDirection_) * (1.0F - carouselEase()) * 90.0F;

    for (float x = 0.0F; x < 400.0F; x += 20.0F) {
        C2D_DrawRectSolid(x, 0.0F, 0.01F, 1.0F, 240.0F, C2D_Color32(105, 180, 116, 35));
    }
    drawIcon(profile, 200.0F + eyeOffset + slide, 54.0F, 62.0F, 0.12F);
    drawPill(252.0F + slide, 22.0F, 88.0F, 18.0F, 0.2F, profile.cartridge ? CursorGreen : Muted);
    ui().drawCentered(text(profile.cartridge ? TextId::CartridgeLabel : TextId::DigitalLabel), 296.0F + slide, 26.0F,
                      0.34F, Surface);
    if (profile.cartridgeEmpty) {
        C2D_DrawRectSolid(0.0F, 91.0F, 0.1F, 400.0F, 33.0F, Muted);
        const bool scanning = app_.saveLoadService_.running()
            && app_.saveLoadService_.operation() == SaveLoadService::Operation::RescanCartridge;
        if (scanning) {
            const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(secondsNow()) * 6.0F);
            C2D_DrawCircleSolid(200.0F + eyeOffset + slide, 54.0F, 0.35F, 6.0F + pulse * 3.0F, Surface);
            ui().drawCentered(text(TextId::StatusCheckingCartridgeSlot), 200.0F + slide, 99.0F, 0.60F, Surface);
        } else {
            ui().drawCentered(text(TextId::NoCartridgeInserted), 200.0F + slide, 99.0F, 0.68F, Surface);
            ui().drawCentered(text(TextId::InsertCartridgeToLoad), 160.0F, 160.0F, 0.48F, Muted);
        }
        return;
    }
    const GameDescriptor& game = supportedGames()[profile.catalogIndex];
    C2D_DrawRectSolid(0.0F, 91.0F, 0.1F, 400.0F, 33.0F, gameVisual(game.code).primary);
    ui().drawCentered(game.name, 200.0F + slide, 99.0F, 0.68F, Surface);
    drawSaveDetails(profile);
}

void GameSelectScreen::render() {
    ui().drawCentered(text(TextId::ChooseTitle), 160.0F, 18.0F, 0.62F, Ink);
    drawPill(4.0F, 202.0F, 60.0F, 26.0F, 0.1F, Surface);
    ui().drawCentered(text(TextId::Logout), 34.0F, 208.0F, 0.38F, Error);
    if (games_.empty()) {
        ui().drawCentered(app_.status_, 160.0F, 106.0F, 0.44F, Error);
        return;
    }
    const std::size_t count = games_.size();
    if (count > 1) {
        drawIcon(games_[previousIndex(index_, count)], 56.0F, 94.0F, 42.0F, 0.1F);
        drawIcon(games_[(index_ + 1) % count], 264.0F, 94.0F, 42.0F, 0.1F);
        ui().drawText("<", 18.0F, 86.0F, 0.72F, Muted);
        ui().drawText(">", 294.0F, 86.0F, 0.72F, Muted);
    }
    const GameProfile& selected = games_[index_];
    drawIcon(selected, 160.0F, 104.0F, 82.0F, 0.2F);
    ui().drawCentered(selected.cartridgeEmpty ? text(TextId::NoCartridgeInserted)
                                              : supportedGames()[selected.catalogIndex].name,
                      160.0F, 157.0F, 0.54F, Ink);
    ui().drawCentered(std::to_string(index_ + 1) + " / " + std::to_string(count), 160.0F, 181.0F, 0.38F, Muted);
    drawHintChip(76.0F, 208.0F, "A", "Select");
    drawHintChip(152.0F, 208.0F, "L/R", "Browse");
    drawHintChip(230.0F, 208.0F, "X", "Rescan");
}
