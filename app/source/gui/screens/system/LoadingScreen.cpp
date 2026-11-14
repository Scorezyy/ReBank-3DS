#include "gui/screens/system/LoadingScreen.hpp"

#include "gui/widgets/ByteProgress.hpp"


#include <algorithm>
#include <cmath>

using namespace Gui;

namespace {
constexpr float Easing = 0.15F;
constexpr u32 TrackColor = C2D_Color32(240, 214, 196, 255);
constexpr UiRect Bar{36.0F, 132.0F, 248.0F, 12.0F};

}

LoadingScreen::StageText LoadingScreen::textFor(Stage stage, bool autoLogin) {
    switch (stage) {
        case Stage::CheckingUpdates:
            return {TextId::LoadingCheckingUpdates, TextId::LoadingDetailCheckingUpdates, false};
        case Stage::SigningIn:
            return autoLogin
                ? StageText{TextId::LoadingAutoLoginDetected, TextId::LoadingDetailAutoLoginDetected, false}
                : StageText{TextId::LoadingSigningIn, TextId::LoadingDetailSigningIn, false};
        case Stage::ScanningLibrary:
            return {TextId::LoadingScanningLibrary, TextId::LoadingDetailScanningLibrary, false};
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
    if (app_.navigator.updateRunning()) {
        return Stage::CheckingUpdates;
    }
    if (app_.navigator.authRunning()) {
        return Stage::SigningIn;
    }
    if (app_.account.welcomeBackPending) {
        return Stage::WelcomeBack;
    }
    switch (app_.saveLoads.phase()) {
        case SaveLoadService::Phase::ScanningLibrary:
            return Stage::ScanningLibrary;
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
    return app_.bankLoads.phase() == LoadService::Phase::LoadingBank ? Stage::LoadingBank : Stage::Waiting;
}

int LoadingScreen::serviceProgress(Stage stage) const {
    const bool saveStage = stage >= Stage::ScanningLibrary && stage <= Stage::SearchingPokemon;
    return std::clamp(saveStage ? app_.saveLoads.progress() : app_.bankLoads.progress(), 0, 100);
}

std::string LoadingScreen::welcomeBack() const {
    return std::string(text(TextId::LoadingWelcomeBackPrefix)) + app_.account.username + "!";
}

void LoadingScreen::tick() {
    if (app_.navigator.loadingWork()) {
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

std::string LoadingScreen::headline() const {
    if (shownStage_ == Stage::WelcomeBack) {
        return welcomeBack();
    }
    if (shownStage_ == Stage::CheckingUpdates) {
        switch (app_.update.progress.stage.load()) {
            case UpdateStage::Checking:
                break;
            case UpdateStage::Installed:
                return std::string(text(TextId::UpdateInstalled));
            case UpdateStage::Failed:
                return std::string(text(TextId::UpdateFailed));
            default:
                return format(TextId::UpdateFound, {"v" + app_.update.progress.version});
        }
    }
    return std::string(text(textFor(shownStage_, app_.account.restoringSession).headline));
}

void LoadingScreen::renderTop(float eyeOffset) {
    ui().drawWarmTop(eyeOffset);
    drawLogo(eyeOffset, 14.0F, 0.56F);
    const bool installed = shownStage_ == Stage::CheckingUpdates
        && app_.update.progress.stage.load() == UpdateStage::Installed;
    const bool failed = shownStage_ == Stage::CheckingUpdates
        && app_.update.progress.stage.load() == UpdateStage::Failed;
    if (installed) {
        ui().drawCheckMark(200.0F + eyeOffset, 148.0F, 20.0F);
    } else if (failed) {
        C2D_DrawCircleSolid(200.0F + eyeOffset, 148.0F, 0.3F, 20.0F, Surface);
        ui().drawCentered("!", 200.0F + eyeOffset, 133.0F, 0.9F, WarmBrandDark);
    } else {
        ui().drawSpinner(200.0F + eyeOffset, 148.0F, 16.0F, Surface);
    }
    ui().drawCentered(headline(), 200.0F + eyeOffset, 184.0F, 0.68F, Surface);
}

void LoadingScreen::renderUpdate() {
    const UpdateProgress& progress = app_.update.progress;
    const UpdateStage stage = progress.stage.load();
    if (stage == UpdateStage::Checking) {
        ui().drawCentered(text(TextId::LoadingDetailCheckingUpdates), 160.0F, 76.0F, 0.50F, WarmInk);
        ui().drawBusyBar(Bar, TrackColor);
        return;
    }
    if (stage == UpdateStage::Failed) {
        ui().drawCentered(text(TextId::UpdateFailed), 160.0F, 70.0F, 0.66F, Error);
        ui().drawCentered(text(app_.update.keptCia ? TextId::UpdateManualFbi : TextId::UpdateManualQr), 160.0F, 108.0F,
                          0.46F, WarmInk);
        ui().drawCentered(text(TextId::UpdateContinueSoon), 160.0F, 150.0F, 0.42F, WarmMuted);
        return;
    }
    if (stage == UpdateStage::Installed) {
        ui().drawCentered(text(TextId::UpdateInstalled), 160.0F, 84.0F, 0.66F, WarmInk);
        ui().drawCentered(text(app_.update.homebrew ? TextId::UpdateRestartHomebrew : TextId::UpdateRestartCia), 160.0F,
                          118.0F, 0.46F, WarmMuted);
        return;
    }
    const TextId detail = stage == UpdateStage::Downloading ? TextId::UpdateDownloading
        : stage == UpdateStage::Verifying ? TextId::UpdateVerifying : TextId::UpdateInstalling;
    ui().drawCentered(text(detail), 160.0F, 66.0F, 0.56F, WarmInk);
    const std::uint32_t total = progress.total.load();
    const std::uint32_t received = std::min(progress.received.load(), total);
    const float share = stage == UpdateStage::Verifying || total == 0 ? 1.0F
        : static_cast<float>(received) / static_cast<float>(total);
    ui().drawCentered(std::to_string(static_cast<int>(share * 100.0F + 0.5F)) + "%", 160.0F, 96.0F, 0.66F, WarmBrand);
    ui().drawProgressBar(Bar, share, TrackColor);
    if (total != 0 && stage != UpdateStage::Verifying) {
        ui().drawCentered(kilobyteProgress(received, total),
                          160.0F, 152.0F, 0.50F, WarmMuted);
    }
    ui().drawCentered(text(TextId::PluginKeepPowerOn), 160.0F, 206.0F, 0.42F, WarmMuted);
}

void LoadingScreen::render() {
    ui().drawWarmBackground();
    if (shownStage_ == Stage::CheckingUpdates) {
        renderUpdate();
        return;
    }
    const StageText stage = textFor(shownStage_, app_.account.restoringSession);
    const std::string detail = shownStage_ == Stage::WelcomeBack ? welcomeBack() : std::string(text(stage.detail));
    ui().drawCentered(detail, 160.0F, 76.0F, 0.50F, WarmInk);

    if (stage.determinate) {
        const int percent = static_cast<int>(displayed_ + 0.5F);
        ui().drawCentered(std::string(text(TextId::LoadingProgressLabel)) + ": " + std::to_string(percent) + "%",
                          160.0F, 104.0F, 0.62F, WarmBrand);
        ui().drawProgressBar(Bar, displayed_ / 100.0F, TrackColor);
        return;
    }

    ui().drawBusyBar(Bar, TrackColor);
}
