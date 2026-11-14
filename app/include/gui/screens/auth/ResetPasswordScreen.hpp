#pragma once

#include "gui/screens/auth/AuthFormScreen.hpp"

class ResetPasswordScreen : public AuthFormScreen {
public:
    explicit ResetPasswordScreen(AppContext& app);

private:
    void submit() override;
};
