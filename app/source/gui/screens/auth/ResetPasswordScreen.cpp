#include "gui/screens/auth/ResetPasswordScreen.hpp"

#include "account/InputValidation.hpp"

namespace {
enum ResetField : std::size_t { Email };
}

ResetPasswordScreen::ResetPasswordScreen(AppContext& app)
    : AuthFormScreen(app, ScreenId::ResetPassword, TextId::ResetPassword,
                     {{TextId::Email, {24.0F, 62.0F, 272.0F, 42.0F}, false, 0.0F, {}}}, std::nullopt) {}

void ResetPasswordScreen::submit() {
    if (rejectIf(!isValidEmail(value(Email)), TextId::InvalidEmailTitle, TextId::InvalidEmailMessage)) {
        return;
    }
    app_.navigator.beginAuth(AuthRequest::passwordReset(value(Email)));
}
