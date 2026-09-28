#include "gui/screens/RegisterScreen.hpp"
#include "app/App.hpp"
#include "core/InputValidation.hpp"

namespace {
enum RegisterField : std::size_t { Username, Email, Password };
}

RegisterScreen::RegisterScreen(App& app)
    : AuthFormScreen(app, ScreenId::Register, TextId::CreateAccount,
                     {{TextId::Username, {24.0F, 48.0F, 272.0F, 34.0F}, false, 1.0F, {}},
                      {TextId::Email, {24.0F, 88.0F, 272.0F, 34.0F}, false, -1.0F, {}},
                      {TextId::Password, {24.0F, 128.0F, 272.0F, 34.0F}, true, 1.0F, {}}},
                     UiRect{24.0F, 166.0F, 272.0F, 26.0F}) {}

void RegisterScreen::submit() {
    if (rejectIf(!isValidUsername(value(Username)), TextId::InvalidUsernameTitle, TextId::InvalidUsernameMessage)
        || rejectIf(!isValidEmail(value(Email)), TextId::InvalidEmailTitle, TextId::InvalidEmailMessage)
        || rejectIf(!isValidPassword(value(Password)), TextId::InvalidPasswordTitle, TextId::InvalidPasswordMessage)) {
        return;
    }
    app_.beginAuth(AuthRequest::registration(value(Username), value(Email), value(Password)));
}
