#pragma once

#include "gui/Screen.hpp"

class WelcomeScreen : public Screen {
public:
    using Screen::Screen;

    void update(const InputFrame& input) override;
    void render() override;
};
