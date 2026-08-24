#include "gui/resetpasswordscreen/ResetPasswordScreen.hpp"
#include "app/App.hpp"
#include "gui/Theme.hpp"
#include "gui/elements/AuthFormChrome.hpp"
#include "i18n/Localization.hpp"
#include "network/AuthController.hpp"

#include <array>
#include <cmath>

using namespace Gui;

void ResetPasswordScreen::open() {
    app_.screen_ = App::Screen::ResetPassword;
    focus_ = Focus::Email;
    animationStartedAt_ = svcGetSystemTick();
}

void ResetPasswordScreen::step(int delta) {
    constexpr std::array<Focus, 3> order{Focus::Email, Focus::Submit, Focus::Back};
    int index = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == focus_) {
            index = static_cast<int>(i);
        }
    }
    int next = (index + delta) % static_cast<int>(order.size());
    if (next < 0) {
        next += static_cast<int>(order.size());
    }
    focus_ = order[next];
}

void ResetPasswordScreen::backToWelcome() {
    app_.status_.clear();
    app_.screen_ = App::Screen::Welcome;
    focus_ = Focus::Email;
}

void ResetPasswordScreen::update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    if (app_.authController_.isRunning()) {
        return;
    }

    const auto activate = [&]() {
        switch (focus_) {
            case Focus::Email:
                app_.requestText(email_, app_.localization_.get(TextId::Email), false);
                break;
            case Focus::Submit:
                if (email_.find('@') == std::string::npos) {
                    app_.status_ = "Please enter a valid email address.";
                } else {
                    app_.beginAuth(AuthOperation::ResetPassword, {}, email_, {});
                }
                break;
            case Focus::Back:
                backToWelcome();
                break;
        }
    };

    if ((keysDown & KEY_UP) || circle.dy > 60) {
        step(-1);
    } else if ((keysDown & KEY_DOWN) || circle.dy < -60) {
        step(1);
    }
    if (keysDown & KEY_A) {
        activate();
        return;
    }
    if (keysDown & KEY_B) {
        backToWelcome();
        return;
    }

    if (!touched) {
        (void)keysHeld;
        return;
    }

    if (BackButton.contains(touch)) {
        backToWelcome();
    } else if (UiRect{24.0F, 62.0F, 272.0F, 42.0F}.contains(touch)) {
        focus_ = Focus::Email;
        app_.requestText(email_, app_.localization_.get(TextId::Email), false);
    } else if (SubmitButton.contains(touch)) {
        focus_ = Focus::Submit;
        activate();
    }
}

void ResetPasswordScreen::render() {
    app_.drawButton(BackButton, app_.localization_.get(TextId::Back), false);

    const double t = static_cast<double>(svcGetSystemTick() - animationStartedAt_) / SYSCLOCK_ARM11;
    const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(t) * 3.0F);
    const float accentPulse = 60.0F + pulse * 40.0F;

    drawAuthFormBackdrop(t);

    app_.drawCentered(app_.localization_.get(TextId::ResetPassword), 160.0F, 18.0F, 0.80F, Ink);
    const u32 underline = C2D_Color32(70, 132, 200, static_cast<u8>(accentPulse * 3.5F));
    C2D_DrawRectSolid(112.0F, 38.0F, 0.05F, 96.0F, 2.0F, underline);

    const UiRect emailField{24.0F, 62.0F, 272.0F, 42.0F};
    if (focus_ == Focus::Email) {
        drawFocusRing(emailField, pulse);
    }
    app_.drawField(emailField, app_.localization_.get(TextId::Email), email_, false);

    const UiRect submitRect{SubmitButton.x, SubmitButton.y, SubmitButton.width, SubmitButton.height};
    if (focus_ == Focus::Submit) {
        drawFocusRing(submitRect, pulse);
    }
    app_.drawButton(submitRect, app_.localization_.get(TextId::Submit), true);

    if (app_.authController_.isRunning()) {
        for (int i = 0; i < 3; ++i) {
            const float phase = static_cast<float>(t) * 4.0F + i * 0.6F;
            const float px = 140.0F + i * 14.0F;
            const float py = 226.0F - std::abs(std::sin(phase)) * 6.0F;
            C2D_DrawCircleSolid(px, py, 0.4F, 4.0F, C2D_Color32(70, 132, 200, 255));
        }
    } else if (!app_.status_.empty()) {
        app_.drawCentered(app_.status_, 160.0F, 222.0F, 0.42F, Error);
    }
}

void ResetPasswordScreen::reset() {
    email_.clear();
}
