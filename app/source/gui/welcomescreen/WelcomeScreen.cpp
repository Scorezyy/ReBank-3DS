#include "gui/welcomescreen/WelcomeScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "gui/Theme.hpp"
#include "i18n/Localization.hpp"

using namespace Gui;

void WelcomeScreen::update(u32 keysDown, touchPosition touch, bool touched) {
    (void)keysDown;
    if (!touched) {
        return;
    }

    constexpr UiRect login{24.0F, 86.0F, 272.0F, 46.0F};
    constexpr UiRect registration{24.0F, 144.0F, 272.0F, 46.0F};
    constexpr UiRect reset{70.0F, 202.0F, 180.0F, 28.0F};
    if (login.contains(touch)) {
        Logger::instance().info("Login form opened");
        app_.loginScreen_.open();
    } else if (registration.contains(touch)) {
        Logger::instance().info("Registration form opened");
        app_.registerScreen_.open();
    } else if (reset.contains(touch)) {
        Logger::instance().info("Password reset form opened");
        app_.resetPasswordScreen_.open();
    }
}

void WelcomeScreen::render() {
    app_.drawCentered("ReBank", 160.0F, 30.0F, 1.05F, Ink);
    app_.drawButton({24.0F, 86.0F, 272.0F, 46.0F}, app_.localization_.get(TextId::Login), true);
    app_.drawButton({24.0F, 144.0F, 272.0F, 46.0F}, app_.localization_.get(TextId::Register), false);
    app_.drawCentered(app_.localization_.get(TextId::ForgotPassword), 160.0F, 207.0F, 0.52F, Brand);
    if (!app_.status_.empty()) {
        app_.drawCentered(app_.status_, 160.0F, 226.0F, 0.36F, Error);
    }
}
