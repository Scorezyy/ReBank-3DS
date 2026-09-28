#pragma once

#include "gui/screens/AuthFormScreen.hpp"

class ResetPasswordScreen : public AuthFormScreen {
public:
    explicit ResetPasswordScreen(App& app);

private:
    void submit() override;
};
