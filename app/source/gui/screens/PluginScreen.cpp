#include "gui/screens/PluginScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"

#include <array>
#include <cmath>

using namespace Gui;

namespace {
constexpr UiRect YesButton{24.0F, 120.0F, 128.0F, 56.0F};
constexpr UiRect NoButton{168.0F, 120.0F, 128.0F, 56.0F};
constexpr UiRect DoneButton{24.0F, 150.0F, 272.0F, 44.0F};
constexpr u32 TrackColor = C2D_Color32(255, 226, 184, 110);
constexpr u32 TopError = C2D_Color32(255, 232, 220, 255);
constexpr float BarX = 60.0F;
constexpr float BarY = 168.0F;
constexpr float BarWidth = 280.0F;
constexpr float BarHeight = 12.0F;
constexpr std::uint32_t BytesPerKb = 1024;
constexpr std::array<TextId, 6> Description = {TextId::PluginInfoConnects, TextId::PluginInfoAccount, TextId::PluginInfoTransfer,
    TextId::PluginInfoOnline, TextId::PluginInfoServices, TextId::PluginInfoSafe};
constexpr std::array<TextId, 3> NextSteps = {TextId::PluginNextEnable, TextId::PluginNextKeys, TextId::PluginNextStart};
constexpr std::array<TextId, 2> ReadySteps = {TextId::PluginLoaderEnabled, TextId::PluginNextJustStart};
constexpr std::array<TextId, 3> UpdateLumaSteps = {TextId::PluginLumaTooOld, TextId::PluginLumaUpdate, TextId::PluginLumaThenStart};

}

void PluginScreen::open() {
    phase_ = Phase::Ask;
    result_ = {};
    app_.status_.clear();
    app_.screen_ = ScreenId::Plugin;
}

void PluginScreen::close() {
    app_.screen_ = ScreenId::GameSelect;
}

void PluginScreen::startInstall() {
    phase_ = Phase::Installing;
    const bool started = task_.start([this]() { return PluginInstaller::run(app_.api_, progress_); });
    if (!started) {
        Logger::instance().warning("Plugin install worker creation failed");
        result_ = {PluginInstallOutcome::StorageFailed, {}};
        phase_ = Phase::Failed;
    }
}

void PluginScreen::update(const InputFrame& input) {
    switch (phase_) {
        case Phase::Ask:
            if (input.pressed(KEY_A) || input.tapped(YesButton)) {
                startInstall();
            } else if (input.pressed(KEY_B) || input.tapped(NoButton)) {
                close();
            }
            return;
        case Phase::Installing:
            if (task_.poll(result_)) {
                phase_ = result_.outcome == PluginInstallOutcome::Installed ? Phase::Done : Phase::Failed;
            }
            return;
        case Phase::Done:
        case Phase::Failed:
            if (input.pressed(KEY_A | KEY_B) || input.tapped(DoneButton)) {
                close();
            }
            return;
    }
}

TextId PluginScreen::failureReason() const {
    switch (result_.outcome) {
        case PluginInstallOutcome::Rejected:
            return TextId::PluginFailedRejected;
        case PluginInstallOutcome::StorageFailed:
            return TextId::PluginFailedStorage;
        default:
            return TextId::PluginFailedUnreachable;
    }
}

void PluginScreen::drawDescription(float eyeOffset) {
    ui().drawCentered(text(TextId::PluginTitle), 200.0F + eyeOffset, 18.0F, 0.86F, Surface);
    C2D_DrawRectSolid(140.0F, 50.0F, 0.1F, 120.0F, 2.0F, WarmCream);
    float y = 66.0F;
    for (std::size_t line = 0; line < Description.size(); ++line) {
        const bool lead = line < 2;
        const bool footer = line + 1 == Description.size();
        if (line == 2 || footer) {
            y += 8.0F;
        }
        ui().drawCentered(text(Description[line]), 200.0F, y, lead ? 0.52F : 0.46F, footer ? WarmCream : Surface);
        y += lead ? 22.0F : 20.0F;
    }
}

void PluginScreen::drawProgress(float eyeOffset) {
    const std::uint32_t total = progress_.total.load();
    const std::uint32_t received = progress_.received.load();
    ui().drawSpinner(200.0F + eyeOffset, 64.0F, 24.0F, Surface);
    const TextId headline = total == 0 ? TextId::PluginConnecting
        : received < total ? TextId::PluginDownloading : TextId::PluginInstalling;
    ui().drawCentered(text(headline), 200.0F + eyeOffset, 116.0F, 0.66F, Surface);
    const float share = total == 0 ? 0.0F : static_cast<float>(received > total ? total : received) / static_cast<float>(total);
    ui().drawProgressBar({BarX, BarY, BarWidth, BarHeight}, share, TrackColor);
    if (total != 0) {
        ui().drawCentered(std::to_string(received / BytesPerKb) + " / " + std::to_string(total / BytesPerKb) + " KB",
                          200.0F, 190.0F, 0.52F, Surface);
    }
}

void PluginScreen::drawResult(float eyeOffset) {
    if (phase_ == Phase::Failed) {
        ui().drawCentered(text(TextId::PluginFailedTitle), 200.0F + eyeOffset, 70.0F, 0.78F, TopError);
        ui().drawCentered(text(failureReason()), 200.0F, 120.0F, 0.48F, Surface);
        return;
    }
    ui().drawCheckMark(200.0F + eyeOffset, 48.0F, 24.0F);
    ui().drawCentered(text(TextId::PluginInstalledTitle), 200.0F + eyeOffset, 86.0F, 0.74F, Surface);
    ui().drawCentered(format(TextId::PluginInstalledVersion, {result_.version}), 200.0F, 114.0F, 0.42F, WarmCream);
    float y = 142.0F;
    if (result_.loader == PluginLoaderState::Missing) {
        for (const TextId step : UpdateLumaSteps) {
            ui().drawCentered(text(step), 200.0F, y, 0.46F, TopError);
            y += 22.0F;
        }
        return;
    }
    if (result_.loader == PluginLoaderState::Enabled) {
        for (const TextId step : ReadySteps) {
            ui().drawCentered(text(step), 200.0F, y, 0.46F, Surface);
            y += 22.0F;
        }
        return;
    }
    for (const TextId step : NextSteps) {
        ui().drawCentered(text(step), 200.0F, y, 0.46F, Surface);
        y += 22.0F;
    }
}

void PluginScreen::renderTop(float eyeOffset) {
    ui().drawWarmTop(eyeOffset);
    switch (phase_) {
        case Phase::Ask:
            drawDescription(eyeOffset);
            return;
        case Phase::Installing:
            drawProgress(eyeOffset);
            return;
        case Phase::Done:
        case Phase::Failed:
            drawResult(eyeOffset);
            return;
    }
}

void PluginScreen::render() {
    ui().drawWarmBackground();
    switch (phase_) {
        case Phase::Ask:
            ui().drawCentered(text(TextId::PluginInstallQuestion), 160.0F, 52.0F, 0.78F, WarmInk);
            ui().drawWarmButton(YesButton, std::string("A  ") + std::string(text(TextId::PluginYes)), true);
            ui().drawWarmButton(NoButton, std::string("B  ") + std::string(text(TextId::PluginNo)), false);
            return;
        case Phase::Installing:
            ui().drawCentered(text(TextId::PluginPleaseWait), 160.0F, 90.0F, 0.66F, WarmInk);
            ui().drawCentered(text(TextId::PluginKeepPowerOn), 160.0F, 124.0F, 0.44F, WarmMuted);
            return;
        case Phase::Done:
        case Phase::Failed:
            ui().drawCentered(text(phase_ == Phase::Done ? TextId::PluginInstalledTitle : TextId::PluginFailedTitle),
                              160.0F, 80.0F, 0.62F, phase_ == Phase::Done ? WarmInk : Error);
            ui().drawWarmButton(DoneButton, text(TextId::Ok), true);
            return;
    }
}
