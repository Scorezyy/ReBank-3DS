#pragma once

#include "gui/screens/AuthFormScreen.hpp"

class RegisterScreen : public AuthFormScreen {
public:
    explicit RegisterScreen(App& app);

private:
    void submit() override;
};
