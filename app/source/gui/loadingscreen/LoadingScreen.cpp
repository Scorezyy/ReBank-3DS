#include "gui/loadingscreen/LoadingScreen.hpp"
#include "app/App.hpp"
#include "gui/Theme.hpp"
#include "i18n/Localization.hpp"

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
    if (app_.welcomeBackPending_) {
        return Stage::WelcomeBack;
    }
    switch (app_.saveLoadService_.phase()) {
        case SaveLoadService::Phase::SearchingGames: return Stage::SearchingGames;
        case SaveLoadService::Phase::ReadingIcons: return Stage::ReadingIcons;
        case SaveLoadService::Phase::ReadingSave: return Stage::ReadingSave;
        case SaveLoadService::Phase::SearchingPokemon: return Stage::SearchingPokemon;
        default: break;
    }
    switch (app_.loadService_.phase()) {
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

    const Localization& localization = app_.localization_;
    const Stage stage = currentStage();
    std::string message(localization.get(TextId::LoadingWait));
    switch (stage) {
        case Stage::CheckingUpdates: message = localization.get(TextId::LoadingCheckingUpdates); break;
        case Stage::SigningIn:
            message = app_.bootAutoLoginInProgress_
                ? localization.get(TextId::LoadingAutoLoginDetected)
                : localization.get(TextId::LoadingSigningIn);
            break;
        case Stage::SearchingGames: message = localization.get(TextId::LoadingSearchingGames); break;
        case Stage::ReadingIcons: message = localization.get(TextId::LoadingReadingIcons); break;
        case Stage::ReadingSave: message = localization.get(TextId::LoadingReadingSave); break;
        case Stage::SearchingPokemon: message = localization.get(TextId::LoadingSearchingPokemon); break;
        case Stage::LoadingBank: message = localization.get(TextId::LoadingBankData); break;
        case Stage::WelcomeBack:
            message = std::string(localization.get(TextId::LoadingWelcomeBackPrefix)) + app_.accountUsername_ + "!";
            break;
        case Stage::Waiting: break;
    }
    app_.drawCentered(message, 200.0F + eyeOffset, 164.0F, 0.68F, Ink);
}

void LoadingScreen::render() {
    const Localization& localization = app_.localization_;
    const Stage stage = currentStage();
    std::string detail(localization.get(TextId::LoadingDetailInitializing));
    switch (stage) {
        case Stage::CheckingUpdates: detail = localization.get(TextId::LoadingDetailCheckingUpdates); break;
        case Stage::SigningIn:
            detail = app_.bootAutoLoginInProgress_
                ? localization.get(TextId::LoadingDetailAutoLoginDetected)
                : localization.get(TextId::LoadingDetailSigningIn);
            break;
        case Stage::SearchingGames: detail = localization.get(TextId::LoadingDetailSearchingGames); break;
        case Stage::ReadingIcons: detail = localization.get(TextId::LoadingDetailReadingIcons); break;
        case Stage::ReadingSave: detail = localization.get(TextId::LoadingDetailReadingSave); break;
        case Stage::SearchingPokemon: detail = localization.get(TextId::LoadingDetailSearchingPokemon); break;
        case Stage::LoadingBank: detail = localization.get(TextId::LoadingDetailLoadingBank); break;
        case Stage::WelcomeBack:
            detail = std::string(localization.get(TextId::LoadingWelcomeBackPrefix)) + app_.accountUsername_ + "!";
            break;
        case Stage::Waiting: break;
    }
    app_.drawCentered(detail, 160.0F, 76.0F, 0.50F, Ink);

    const bool determinate = stage != Stage::CheckingUpdates
        && stage != Stage::SigningIn
        && stage != Stage::Waiting
        && stage != Stage::WelcomeBack;

    if (determinate) {
        const bool isSaveStage = stage == Stage::SearchingGames || stage == Stage::ReadingIcons
            || stage == Stage::ReadingSave || stage == Stage::SearchingPokemon;
        float& displayedProgress = isSaveStage
            ? app_.saveLoadService_.displayedProgress() : app_.loadService_.displayedProgress();
        const float target = static_cast<float>(std::clamp(
            isSaveStage ? app_.saveLoadService_.progress() : app_.loadService_.progress(), 0, 100));
        if (target < displayedProgress) {
            displayedProgress = target;
        } else {
            displayedProgress += (target - displayedProgress) * 0.15F;
            if (target - displayedProgress < 0.5F) {
                displayedProgress = target;
            }
        }
        const int percent = static_cast<int>(displayedProgress + 0.5F);
        app_.drawCentered(std::string(localization.get(TextId::LoadingProgressLabel)) + ": "
                     + std::to_string(percent) + "%",
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
