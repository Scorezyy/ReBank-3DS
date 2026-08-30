#include "gui/loginscreen/LoginScreen.hpp"
#include "app/App.hpp"
#include "gui/Theme.hpp"
#include "gui/elements/AuthFormChrome.hpp"
#include "i18n/Localization.hpp"
#include "network/AuthController.hpp"

#include <algorithm>
#include <array>
#include <cmath>

using namespace Gui;

void LoginScreen::open() {
    app_.screen_ = App::Screen::Login;
    focus_ = Focus::Username;
    animationStartedAt_ = svcGetSystemTick();
}

bool LoginScreen::usernameValid() const {
    return isValidUsername(username_);
}

void LoginScreen::step(int delta) {
    constexpr std::array<Focus, 5> order{
        Focus::Username, Focus::Password, Focus::AutoLogin, Focus::Submit, Focus::Back
    };
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

void LoginScreen::backToWelcome() {
    password_.clear();
    app_.status_.clear();
    app_.screen_ = App::Screen::Welcome;
    focus_ = Focus::Username;
}

void LoginScreen::update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    if (app_.authController_.isRunning()) {
        return;
    }

    const auto activate = [&]() {
        switch (focus_) {
            case Focus::Username:
                app_.requestText(username_, app_.localization_.get(TextId::Username), false);
                break;
            case Focus::Password:
                app_.requestText(password_, app_.localization_.get(TextId::Password), true);
                break;
            case Focus::AutoLogin:
                app_.toggleAutoLogin();
                break;
            case Focus::Submit:
                if (!usernameValid()) {
                    app_.showError("INVALID USERNAME", "Username must be 3-32 letters, numbers, _ or -.");
                } else if (password_.size() < 10) {
                    app_.showError("INVALID PASSWORD", "Password must contain at least 10 characters.");
                } else {
                    app_.beginAuth(AuthOperation::Login, username_, {}, password_);
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
    if (keysDown & KEY_Y) {
        app_.toggleAutoLogin();
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
        focus_ = Focus::Username;
        app_.requestText(username_, app_.localization_.get(TextId::Username), false);
    } else if (UiRect{24.0F, 114.0F, 272.0F, 42.0F}.contains(touch)) {
        focus_ = Focus::Password;
        app_.requestText(password_, app_.localization_.get(TextId::Password), true);
    } else if (UiRect{24.0F, 162.0F, 272.0F, 26.0F}.contains(touch)) {
        focus_ = Focus::AutoLogin;
        app_.toggleAutoLogin();
    } else if (SubmitButton.contains(touch)) {
        focus_ = Focus::Submit;
        activate();
    }
}

void LoginScreen::render() {
    app_.drawButton(BackButton, app_.localization_.get(TextId::Back), false);

    const double t = static_cast<double>(svcGetSystemTick() - animationStartedAt_) / SYSCLOCK_ARM11;
    const float slide = std::max(0.0F, 20.0F - static_cast<float>(t) * 90.0F);
    const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(t) * 3.0F);
    const float accentPulse = 60.0F + pulse * 40.0F;

    drawAuthFormBackdrop(t);

    app_.drawCentered(app_.localization_.get(TextId::Login), 160.0F, 18.0F, 0.80F, Ink);
    const u32 underline = C2D_Color32(70, 132, 200, static_cast<u8>(accentPulse * 3.5F));
    C2D_DrawRectSolid(112.0F, 38.0F, 0.05F, 96.0F, 2.0F, underline);

    const UiRect usernameField{24.0F + slide, 62.0F, 272.0F, 42.0F};
    if (focus_ == Focus::Username) {
        drawFocusRing(usernameField, pulse);
    }
    app_.drawField(usernameField, app_.localization_.get(TextId::Username), username_, false);

    const UiRect pwField{24.0F + slide, 114.0F, 272.0F, 42.0F};
    if (focus_ == Focus::Password) {
        drawFocusRing(pwField, pulse);
    }
    app_.drawField(pwField, app_.localization_.get(TextId::Password), password_, true);

    const UiRect autoRect{24.0F, 162.0F, 272.0F, 26.0F};
    if (focus_ == Focus::AutoLogin) {
        drawFocusRing(autoRect, pulse);
    }
    drawAutoLoginCheckbox(app_.ui(), autoRect, app_.autoLogin_);

    const UiRect submitRect{SubmitButton.x, SubmitButton.y, SubmitButton.width, SubmitButton.height};
    if (focus_ == Focus::Submit) {
        drawFocusRing(submitRect, pulse);
    }
    app_.drawButton(submitRect, app_.localization_.get(TextId::Submit), true);

    if (app_.authController_.isRunning()) {
        drawSubmitTypingDots(t);
    }
}

void LoginScreen::reset() {
    username_.clear();
    password_.clear();
}
