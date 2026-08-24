#include "gui/loadingscreen/LoadingScreen.hpp"
#include "app/App.hpp"
#include "gui/Theme.hpp"

#include <algorithm>
#include <cmath>

using namespace Gui;

LoadingScreen::Stage LoadingScreen::currentStage() const {
    if (app_.updateController_.isRunning()) {
        return Stage::CheckingUpdates;
    }
    if (app_.authController_.isRunning()) {
        return Stage::SigningIn;
    }
    switch (app_.loadService_.phase()) {
        case LoadService::Phase::SearchingGames: return Stage::SearchingGames;
        case LoadService::Phase::ReadingIcons: return Stage::ReadingIcons;
        case LoadService::Phase::ReadingSave: return Stage::ReadingSave;
        case LoadService::Phase::SearchingPokemon: return Stage::SearchingPokemon;
        case LoadService::Phase::LoadingBank: return Stage::LoadingBank;
        default: return Stage::Waiting;
    }
}

void LoadingScreen::renderTop(float eyeOffset) {
    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float angle = static_cast<float>(seconds) * 4.5F;
    app_.drawCentered("ReBank", 200.0F + eyeOffset, 30.0F, 1.05F, Ink);
    for (int index = 0; index < 10; ++index) {
        const float phase = angle + static_cast<float>(index) * 0.6283185F;
        const float x = 200.0F + eyeOffset + std::cos(phase) * 30.0F;
        const float y = 105.0F + std::sin(phase) * 30.0F;
        const std::uint8_t alpha = static_cast<std::uint8_t>(70 + index * 18);
        C2D_DrawCircleSolid(x, y, 0.3F, 4.5F, C2D_Color32(31, 145, 94, alpha));
    }

    std::string_view message = "Bitte warten...";
    switch (currentStage()) {
        case Stage::CheckingUpdates: message = "Pruefe Updates..."; break;
        case Stage::SigningIn: message = "Anmelden..."; break;
        case Stage::SearchingGames: message = "Suche Spielstaende..."; break;
        case Stage::ReadingIcons: message = "Lade Spielbilder..."; break;
        case Stage::ReadingSave: message = "Lade Spielstand..."; break;
        case Stage::SearchingPokemon: message = "Suche Pokemon..."; break;
        case Stage::LoadingBank: message = "Lade Bank-Daten..."; break;
        case Stage::Waiting: break;
    }
    app_.drawCentered(message, 200.0F + eyeOffset, 164.0F, 0.68F, Ink);
}

void LoadingScreen::render() {
    const Stage stage = currentStage();
    std::string_view detail = "Initialisiere...";
    switch (stage) {
        case Stage::CheckingUpdates: detail = "Lade und verifiziere neue Versionen sicher..."; break;
        case Stage::SigningIn: detail = "Pruefe Server und Sitzung..."; break;
        case Stage::SearchingGames: detail = "Pruefe Cartridge und installierte Spiele..."; break;
        case Stage::ReadingIcons: detail = "Lese originale Spielbilder..."; break;
        case Stage::ReadingSave: detail = "Oeffne lokalen Spielstand..."; break;
        case Stage::SearchingPokemon: detail = "Lese Box und Pokemon..."; break;
        case Stage::LoadingBank: detail = "Verbinde mit deiner Bank..."; break;
        case Stage::Waiting: break;
    }
    app_.drawCentered(detail, 160.0F, 76.0F, 0.50F, Ink);

    const bool determinate = stage != Stage::CheckingUpdates
        && stage != Stage::SigningIn
        && stage != Stage::Waiting;

    if (determinate) {
        float& displayedProgress = app_.loadService_.displayedProgress();
        const float target = static_cast<float>(std::clamp(app_.loadService_.progress(), 0, 100));
        if (target < displayedProgress) {
            displayedProgress = target;
        } else {
            displayedProgress += (target - displayedProgress) * 0.15F;
            if (target - displayedProgress < 0.5F) {
                displayedProgress = target;
            }
        }
        const int percent = static_cast<int>(displayedProgress + 0.5F);
        app_.drawCentered("Progress: " + std::to_string(percent) + "%",
                     160.0F, 110.0F, 0.62F, Ink);
        C2D_DrawRectSolid(36.0F, 138.0F, 0.2F, 248.0F, 10.0F, C2D_Color32(205, 220, 211, 255));
        C2D_DrawRectSolid(36.0F, 138.0F, 0.3F, 248.0F * displayedProgress / 100.0F,
                          10.0F, Brand);
        return;
    }

    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float cycle = std::fmod(static_cast<float>(seconds) * 0.65F, 1.0F);
    C2D_DrawRectSolid(36.0F, 122.0F, 0.2F, 248.0F, 8.0F, C2D_Color32(205, 220, 211, 255));
    C2D_DrawRectSolid(36.0F + cycle * 188.0F, 122.0F, 0.3F, 60.0F, 8.0F, Brand);
    for (int index = 0; index < 3; ++index) {
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(seconds) * 5.0F + static_cast<float>(index) * 1.4F);
        C2D_DrawCircleSolid(140.0F + index * 20.0F, 172.0F, 0.3F,
                            3.0F + pulse * 2.0F, Accent);
    }
}
