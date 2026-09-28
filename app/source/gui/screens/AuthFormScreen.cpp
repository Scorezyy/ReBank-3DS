#include "gui/screens/AuthFormScreen.hpp"
#include "app/App.hpp"

#include <algorithm>
#include <cmath>

using namespace Gui;

namespace {
constexpr u32 FocusBlue = C2D_Color32(70, 132, 200, 255);

void drawBackdrop(double seconds) {
    for (int band = 0; band < 3; ++band) {
        const float y = 40.0F + band * 62.0F + std::sin(static_cast<float>(seconds) + band) * 2.0F;
        C2D_DrawRectSolid(0.0F, y, 0.02F, 320.0F, 2.0F, C2D_Color32(210, 220, 240, 60));
    }
}

void drawFocusRing(const UiRect& rect, float pulse) {
    const u32 ring = C2D_Color32(70, 132, 200, static_cast<u8>(140 + pulse * 100));
    C2D_DrawRectSolid(rect.x - 3.0F, rect.y - 3.0F, 0.08F, rect.width + 6.0F, 3.0F, ring);
    C2D_DrawRectSolid(rect.x - 3.0F, rect.y + rect.height, 0.08F, rect.width + 6.0F, 3.0F, ring);
    C2D_DrawRectSolid(rect.x - 3.0F, rect.y - 3.0F, 0.08F, 3.0F, rect.height + 6.0F, ring);
    C2D_DrawRectSolid(rect.x + rect.width, rect.y - 3.0F, 0.08F, 3.0F, rect.height + 6.0F, ring);
}

void drawCheckbox(UiRenderer& ui, const UiRect& rect, bool checked, std::string_view label) {
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, C2D_Color32(250, 250, 254, 220));
    const float boxX = rect.x + 8.0F;
    const float boxY = rect.y + 5.0F;
    C2D_DrawRectSolid(boxX, boxY, 0.12F, 16.0F, 16.0F, checked ? CursorGreen : C2D_Color32(210, 214, 224, 255));
    C2D_DrawRectSolid(boxX + 2.0F, boxY + 2.0F, 0.13F, 12.0F, 12.0F,
                      checked ? C2D_Color32(80, 200, 120, 255) : C2D_Color32(240, 244, 252, 255));
    if (checked) {
        C2D_DrawRectSolid(boxX + 4.0F, boxY + 8.0F, 0.14F, 3.0F, 4.0F, Surface);
        C2D_DrawRectSolid(boxX + 5.0F, boxY + 10.0F, 0.14F, 8.0F, 3.0F, Surface);
    }
    ui.drawText(label, boxX + 26.0F, rect.y + 7.0F, 0.44F, Ink);
}

void drawTypingDots(double seconds) {
    for (int dot = 0; dot < 3; ++dot) {
        const float phase = static_cast<float>(seconds) * 4.0F + dot * 0.6F;
        C2D_DrawCircleSolid(140.0F + dot * 14.0F, 226.0F - std::abs(std::sin(phase)) * 6.0F, 0.4F, 4.0F, FocusBlue);
    }
}
}

AuthFormScreen::AuthFormScreen(App& app, ScreenId id, TextId title, std::vector<Field> fields,
                               std::optional<UiRect> autoLoginBounds)
    : Screen(app), id_(id), title_(title), fields_(std::move(fields)), autoLoginBounds_(autoLoginBounds) {}

std::size_t AuthFormScreen::itemCount() const {
    return fields_.size() + (autoLoginBounds_ ? 1 : 0) + 2;
}

AuthFormScreen::Item AuthFormScreen::itemAt(std::size_t index) const {
    if (index < fields_.size()) {
        return Item::Field;
    }
    const std::size_t trailing = itemCount() - index;
    if (trailing == 1) {
        return Item::Back;
    }
    return trailing == 2 ? Item::Submit : Item::AutoLogin;
}

std::size_t AuthFormScreen::indexOf(Item item) const {
    switch (item) {
        case Item::Back:
            return itemCount() - 1;
        case Item::Submit:
            return itemCount() - 2;
        case Item::AutoLogin:
            return fields_.size();
        case Item::Field:
            break;
    }
    return 0;
}

void AuthFormScreen::open() {
    app_.screen_ = id_;
    focus_ = 0;
    animationStartedAt_ = svcGetSystemTick();
}

void AuthFormScreen::reset() {
    for (Field& field : fields_) {
        field.value.clear();
    }
}

void AuthFormScreen::back() {
    for (Field& field : fields_) {
        if (field.secret) {
            field.value.clear();
        }
    }
    app_.status_.clear();
    app_.screen_ = ScreenId::Welcome;
    focus_ = 0;
}

bool AuthFormScreen::rejectIf(bool invalid, TextId title, TextId message) {
    if (invalid) {
        app_.showError(title, message);
    }
    return invalid;
}

void AuthFormScreen::edit(std::size_t field) {
    focus_ = field;
    app_.requestText(fields_[field].value, text(fields_[field].label), fields_[field].secret);
}

void AuthFormScreen::activate() {
    switch (itemAt(focus_)) {
        case Item::Field:
            edit(focus_);
            return;
        case Item::AutoLogin:
            app_.toggleAutoLogin();
            return;
        case Item::Submit:
            submit();
            return;
        case Item::Back:
            back();
            return;
    }
}

void AuthFormScreen::update(const InputFrame& input) {
    if (app_.authController_.isRunning()) {
        return;
    }
    if (const int step = input.verticalStep(); step != 0) {
        const auto count = static_cast<int>(itemCount());
        focus_ = static_cast<std::size_t>(((static_cast<int>(focus_) + step) % count + count) % count);
    }
    if (input.pressed(KEY_A)) {
        activate();
        return;
    }
    if (autoLoginBounds_ && input.pressed(KEY_Y)) {
        app_.toggleAutoLogin();
    }
    if (input.pressed(KEY_B) || input.tapped(BackButton)) {
        back();
        return;
    }
    for (std::size_t field = 0; field < fields_.size(); ++field) {
        if (input.tapped(fields_[field].bounds)) {
            edit(field);
            return;
        }
    }
    if (autoLoginBounds_ && input.tapped(*autoLoginBounds_)) {
        focus(Item::AutoLogin);
        app_.toggleAutoLogin();
    } else if (input.tapped(SubmitButton)) {
        focus(Item::Submit);
        submit();
    }
}

void AuthFormScreen::render() {
    ui().drawButton(BackButton, text(TextId::Back), false);

    const double seconds = static_cast<double>(svcGetSystemTick() - animationStartedAt_) / SYSCLOCK_ARM11;
    const float slide = std::max(0.0F, 20.0F - static_cast<float>(seconds) * 90.0F);
    const float pulse = 0.5F + 0.5F * std::sin(static_cast<float>(seconds) * 3.0F);
    const auto underlineAlpha = static_cast<u8>(std::min(255.0F, (60.0F + pulse * 40.0F) * 3.5F));

    drawBackdrop(seconds);
    ui().drawCentered(text(title_), 160.0F, 18.0F, 0.80F, Ink);
    C2D_DrawRectSolid(112.0F, 38.0F, 0.05F, 96.0F, 2.0F, C2D_Color32(70, 132, 200, underlineAlpha));

    for (std::size_t index = 0; index < fields_.size(); ++index) {
        const Field& field = fields_[index];
        UiRect bounds = field.bounds;
        bounds.x += slide * field.slideDirection;
        if (focus_ == index) {
            drawFocusRing(bounds, pulse);
        }
        ui().drawField(bounds, text(field.label), field.value, field.secret);
    }
    if (autoLoginBounds_) {
        if (itemAt(focus_) == Item::AutoLogin) {
            drawFocusRing(*autoLoginBounds_, pulse);
        }
        drawCheckbox(ui(), *autoLoginBounds_, app_.autoLogin_, text(TextId::AutoLoginToggle));
    }
    if (itemAt(focus_) == Item::Submit) {
        drawFocusRing(SubmitButton, pulse);
    }
    ui().drawButton(SubmitButton, text(TextId::Submit), true);

    if (app_.authController_.isRunning()) {
        drawTypingDots(seconds);
    }
}
