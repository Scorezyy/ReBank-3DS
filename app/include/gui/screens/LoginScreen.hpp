#pragma once

#include "gui/screens/AuthFormScreen.hpp"

class LoginScreen : public AuthFormScreen {
public:
    explicit LoginScreen(App& app);

private:
    void submit() override;
};
