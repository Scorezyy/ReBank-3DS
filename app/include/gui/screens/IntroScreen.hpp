#pragma once

#include "gui/Screen.hpp"

class IntroScreen : public Screen {
public:
    using Screen::Screen;

    void update(const InputFrame& input) override;
    void render() override;
};
