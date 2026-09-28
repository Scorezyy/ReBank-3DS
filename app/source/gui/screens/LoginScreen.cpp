#include "gui/screens/LoginScreen.hpp"
#include "app/App.hpp"
#include "core/InputValidation.hpp"

namespace {
enum LoginField : std::size_t { Username, Password };
}

LoginScreen::LoginScreen(App& app)
    : AuthFormScreen(app, ScreenId::Login, TextId::Login,
                     {{TextId::Username, {24.0F, 62.0F, 272.0F, 42.0F}, false, 1.0F, {}},
                      {TextId::Password, {24.0F, 114.0F, 272.0F, 42.0F}, true, 1.0F, {}}},
                     UiRect{24.0F, 162.0F, 272.0F, 26.0F}) {}

void LoginScreen::submit() {
    if (rejectIf(!isValidUsername(value(Username)), TextId::InvalidUsernameTitle, TextId::InvalidUsernameMessage)
        || rejectIf(!isValidPassword(value(Password)), TextId::InvalidPasswordTitle, TextId::InvalidPasswordMessage)) {
        return;
    }
    app_.beginAuth(AuthRequest::login(value(Username), value(Password)));
}
