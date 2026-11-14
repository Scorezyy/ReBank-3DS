#pragma once

#include "gui/screens/auth/AuthFormScreen.hpp"

class LoginScreen : public AuthFormScreen {
public:
    explicit LoginScreen(AppContext& app);

private:
    void submit() override;
};
