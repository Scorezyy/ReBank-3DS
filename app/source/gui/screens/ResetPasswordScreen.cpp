#include "gui/screens/ResetPasswordScreen.hpp"
#include "app/App.hpp"
#include "core/InputValidation.hpp"

namespace {
enum ResetField : std::size_t { Email };
}

ResetPasswordScreen::ResetPasswordScreen(App& app)
    : AuthFormScreen(app, ScreenId::ResetPassword, TextId::ResetPassword,
                     {{TextId::Email, {24.0F, 62.0F, 272.0F, 42.0F}, false, 0.0F, {}}}, std::nullopt) {}

void ResetPasswordScreen::submit() {
    if (rejectIf(!isValidEmail(value(Email)), TextId::InvalidEmailTitle, TextId::InvalidEmailMessage)) {
        return;
    }
    app_.beginAuth(AuthRequest::passwordReset(value(Email)));
}
