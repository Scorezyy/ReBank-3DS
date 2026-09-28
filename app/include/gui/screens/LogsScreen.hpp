#pragma once

#include "gui/Screen.hpp"

class LogsScreen : public Screen {
public:
    using Screen::Screen;

    void update(const InputFrame&) override {}
    void render() override;
};
