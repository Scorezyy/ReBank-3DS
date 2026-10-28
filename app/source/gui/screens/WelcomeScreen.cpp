#include "gui/screens/WelcomeScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"

using namespace Gui;

namespace {
constexpr UiRect LoginButton{24.0F, 86.0F, 272.0F, 46.0F};
constexpr UiRect RegisterButton{24.0F, 144.0F, 272.0F, 46.0F};
constexpr UiRect ForgotPasswordLink{70.0F, 202.0F, 180.0F, 28.0F};
}

void WelcomeScreen::update(const InputFrame& input) {
    if (input.tapped(LoginButton)) {
        Logger::instance().info("Login form opened");
        app_.loginScreen_.open();
    } else if (input.tapped(RegisterButton)) {
        Logger::instance().info("Registration form opened");
        app_.registerScreen_.open();
    } else if (input.tapped(ForgotPasswordLink)) {
        Logger::instance().info("Password reset form opened");
        app_.resetPasswordScreen_.open();
    }
}

void WelcomeScreen::render() {
    ui().drawWarmBackground();
    if (app_.resources_.logo) {
        const C2D_Image emblem = C2D_SpriteSheetGetImage(app_.resources_.logo, LogoSmallIdx);
        C2D_DrawImageAt(emblem, 24.0F, 12.0F, 0.5F);
    }
    ui().drawText("ReBank", 96.0F, 20.0F, 0.95F, WarmInk);
    ui().drawText(text(TextId::Tagline), 97.0F, 52.0F, 0.42F, WarmMuted);
    ui().drawWarmButton(LoginButton, text(TextId::Login), true);
    ui().drawWarmButton(RegisterButton, text(TextId::Register), false);
    ui().drawCentered(text(TextId::ForgotPassword), 160.0F, 207.0F, 0.52F, WarmBrand);
    if (!app_.status_.empty()) {
        ui().drawCentered(app_.status_, 160.0F, 226.0F, 0.36F, Error);
    }
}
