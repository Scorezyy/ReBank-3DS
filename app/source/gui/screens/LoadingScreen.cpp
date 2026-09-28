#include "gui/screens/LoadingScreen.hpp"
#include "app/App.hpp"

#include <algorithm>
#include <cmath>

using namespace Gui;

namespace {
constexpr float Easing = 0.15F;
constexpr u32 TrackColor = C2D_Color32(205, 220, 211, 255);

double secondsNow() {
    return static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
}
}

LoadingScreen::StageText LoadingScreen::textFor(Stage stage, bool autoLogin) {
    switch (stage) {
        case Stage::CheckingUpdates:
            return {TextId::LoadingCheckingUpdates, TextId::LoadingDetailCheckingUpdates, false};
        case Stage::SigningIn:
            return autoLogin
                ? StageText{TextId::LoadingAutoLoginDetected, TextId::LoadingDetailAutoLoginDetected, false}
                : StageText{TextId::LoadingSigningIn, TextId::LoadingDetailSigningIn, false};
        case Stage::SearchingGames:
            return {TextId::LoadingSearchingGames, TextId::LoadingDetailSearchingGames, true};
        case Stage::ReadingIcons:
            return {TextId::LoadingReadingIcons, TextId::LoadingDetailReadingIcons, true};
        case Stage::ReadingSave:
            return {TextId::LoadingReadingSave, TextId::LoadingDetailReadingSave, true};
        case Stage::SearchingPokemon:
            return {TextId::LoadingSearchingPokemon, TextId::LoadingDetailSearchingPokemon, true};
        case Stage::LoadingBank:
            return {TextId::LoadingBankData, TextId::LoadingDetailLoadingBank, true};
        case Stage::WelcomeBack:
            return {TextId::LoadingWelcomeBackPrefix, TextId::LoadingWelcomeBackPrefix, false};
        case Stage::Waiting:
            break;
    }
    return {TextId::LoadingWait, TextId::LoadingDetailInitializing, false};
}

LoadingScreen::Stage LoadingScreen::currentStage() const {
    if (app_.updateTask_.running()) {
        return Stage::CheckingUpdates;
    }
    if (app_.authController_.isRunning()) {
        return Stage::SigningIn;
    }
    if (app_.welcomeBackPending_) {
        return Stage::WelcomeBack;
    }
    switch (app_.saveLoadService_.phase()) {
        case SaveLoadService::Phase::SearchingGames:
            return Stage::SearchingGames;
        case SaveLoadService::Phase::ReadingIcons:
            return Stage::ReadingIcons;
        case SaveLoadService::Phase::ReadingSave:
            return Stage::ReadingSave;
        case SaveLoadService::Phase::SearchingPokemon:
            return Stage::SearchingPokemon;
        case SaveLoadService::Phase::Idle:
            break;
    }
    return app_.loadService_.phase() == LoadService::Phase::LoadingBank ? Stage::LoadingBank : Stage::Waiting;
}

int LoadingScreen::serviceProgress(Stage stage) const {
    const bool saveStage = stage >= Stage::SearchingGames && stage <= Stage::SearchingPokemon;
    return std::clamp(saveStage ? app_.saveLoadService_.progress() : app_.loadService_.progress(), 0, 100);
}

std::string LoadingScreen::welcomeBack() const {
    return std::string(text(TextId::LoadingWelcomeBackPrefix)) + app_.accountUsername_ + "!";
}

void LoadingScreen::tick() {
    if (app_.isLoadingWork()) {
        const Stage stage = currentStage();
        if (stage != shownStage_) {
            shownStage_ = stage;
            displayed_ = 0.0F;
        }
        const bool determinate = textFor(stage, false).determinate;
        const float target = determinate ? static_cast<float>(serviceProgress(stage)) : 0.0F;
        displayed_ = target < displayed_ ? target : displayed_ + (target - displayed_) * Easing;
        if (target - displayed_ < 0.5F) {
            displayed_ = target;
        }
        finishing_ = determinate;
        return;
    }
    if (finishing_) {
        displayed_ += (100.0F - displayed_) * Easing;
        if (100.0F - displayed_ >= 0.5F) {
            return;
        }
    }
    finishing_ = false;
    shownStage_ = Stage::Waiting;
    displayed_ = 0.0F;
}

void LoadingScreen::renderTop(float eyeOffset) {
    const float angle = static_cast<float>(secondsNow()) * 4.5F;
    ui().drawCentered("ReBank", 200.0F + eyeOffset, 30.0F, 1.05F, Ink);
    for (int index = 0; index < 10; ++index) {
        const float phase = angle + static_cast<float>(index) * 0.6283185F;
        C2D_DrawCircleSolid(200.0F + eyeOffset + std::cos(phase) * 30.0F, 105.0F + std::sin(phase) * 30.0F, 0.3F,
                            4.5F, C2D_Color32(31, 145, 94, static_cast<u8>(70 + index * 18)));
    }
    const std::string headline = shownStage_ == Stage::WelcomeBack
        ? welcomeBack()
        : std::string(text(textFor(shownStage_, app_.bootAutoLoginInProgress_).headline));
    ui().drawCentered(headline, 200.0F + eyeOffset, 164.0F, 0.68F, Ink);
}

void LoadingScreen::render() {
    const StageText stage = textFor(shownStage_, app_.bootAutoLoginInProgress_);
    const std::string detail = shownStage_ == Stage::WelcomeBack ? welcomeBack() : std::string(text(stage.detail));
    ui().drawCentered(detail, 160.0F, 76.0F, 0.50F, Ink);

    if (stage.determinate) {
        const int percent = static_cast<int>(displayed_ + 0.5F);
        ui().drawCentered(std::string(text(TextId::LoadingProgressLabel)) + ": " + std::to_string(percent) + "%",
                          160.0F, 110.0F, 0.62F, Ink);
        C2D_DrawRectSolid(36.0F, 138.0F, 0.2F, 248.0F, 10.0F, TrackColor);
        C2D_DrawRectSolid(36.0F, 138.0F, 0.3F, 248.0F * displayed_ / 100.0F, 10.0F, Brand);
        return;
    }

    const double seconds = secondsNow();
    const float cycle = std::fmod(static_cast<float>(seconds) * 0.65F, 1.0F);
    C2D_DrawRectSolid(36.0F, 122.0F, 0.2F, 248.0F, 8.0F, TrackColor);
    C2D_DrawRectSolid(36.0F + cycle * 188.0F, 122.0F, 0.3F, 60.0F, 8.0F, Brand);
    for (int index = 0; index < 3; ++index) {
        const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(seconds) * 5.0F + static_cast<float>(index) * 1.4F);
        C2D_DrawCircleSolid(140.0F + index * 20.0F, 172.0F, 0.3F, 3.0F + pulse * 2.0F, Accent);
    }
}
