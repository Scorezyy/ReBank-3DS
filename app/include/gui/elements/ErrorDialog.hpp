#pragma once

#include "gui/UiRenderer.hpp"

#include <string>

class ErrorDialog {
public:
    void show(std::string title, std::string message);
    void dismiss() { visible_ = false; }
    bool visible() const { return visible_; }
    void render(UiRenderer& ui) const;

private:
    bool visible_ = false;
    std::string title_;
    std::string message_;
};
