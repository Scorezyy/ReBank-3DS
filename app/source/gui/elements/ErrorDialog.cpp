#include "gui/elements/ErrorDialog.hpp"
#include "gui/Theme.hpp"

#include <vector>

using namespace Gui;

void ErrorDialog::show(std::string title, std::string message) {
    title_ = std::move(title);
    message_ = std::move(message);
    visible_ = true;
}

void ErrorDialog::render(UiRenderer& ui) const {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, C2D_Color32(12, 24, 19, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.40F, 284.0F, 208.0F, C2D_Color32(250, 247, 238, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.45F, 7.0F, 208.0F, Error);
    C2D_DrawRectSolid(25.0F, 20.0F, 0.45F, 277.0F, 36.0F, C2D_Color32(255, 225, 214, 255));

    ui.drawText(title_, 36.0F, 30.0F, 0.52F, Error);

    std::vector<std::string> lines;
    std::string remaining = message_.empty()
        ? "An unexpected error occurred. Check rebank.log for details."
        : message_;
    constexpr std::size_t MaxLineLength = 42;
    while (!remaining.empty() && lines.size() < 6) {
        if (remaining.size() <= MaxLineLength) {
            lines.push_back(remaining);
            break;
        }
        std::size_t split = remaining.rfind(' ', MaxLineLength);
        if (split == std::string::npos || split == 0) {
            split = MaxLineLength;
        }
        lines.push_back(remaining.substr(0, split));
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ') {
            remaining.erase(remaining.begin());
        }
    }
    if (!remaining.empty() && !lines.empty()) {
        std::string& last = lines.back();
        if (last.size() > MaxLineLength - 3) {
            last.resize(MaxLineLength - 3);
        }
        last += "...";
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        ui.drawText(lines[index], 36.0F, 76.0F + static_cast<float>(index) * 15.0F, 0.38F, Ink);
    }

    const UiRect okButton{92.0F, 190.0F, 136.0F, 34.0F};
    C2D_DrawRectSolid(okButton.x, okButton.y, 0.46F, okButton.width, okButton.height, Brand);
    C2D_DrawRectSolid(okButton.x + 2.0F, okButton.y + 2.0F, 0.47F,
                      okButton.width - 4.0F, okButton.height - 4.0F, CursorGreen);
    ui.drawCentered("OK", 160.0F, 199.0F, 0.52F, C2D_Color32(255, 255, 255, 255));
    ui.drawCentered("A / B", 268.0F, 201.0F, 0.30F, Muted);
}
