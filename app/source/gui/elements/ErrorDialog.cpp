#include "gui/elements/ErrorDialog.hpp"
#include "gui/Theme.hpp"

#include <string>
#include <vector>

namespace Gui {
namespace {
constexpr std::size_t MaxLineLength = 42;
constexpr std::string_view Ellipsis = "...";

std::vector<std::string> wrapLines(std::string_view text, std::size_t maxLines) {
    std::vector<std::string> lines;
    while (!text.empty() && lines.size() < maxLines) {
        if (text.size() <= MaxLineLength) {
            lines.emplace_back(text);
            return lines;
        }
        std::size_t split = text.rfind(' ', MaxLineLength);
        if (split == std::string_view::npos || split == 0) {
            split = MaxLineLength;
        }
        lines.emplace_back(text.substr(0, split));
        text.remove_prefix(split);
        text.remove_prefix(std::min(text.find_first_not_of(' '), text.size()));
    }
    if (!text.empty() && !lines.empty()) {
        std::string& last = lines.back();
        last.resize(std::min(last.size(), MaxLineLength - Ellipsis.size()));
        last += Ellipsis;
    }
    return lines;
}
}

void drawErrorDialog(UiRenderer& ui, const ErrorNotice& notice, std::string_view fallbackMessage,
                     std::string_view okLabel) {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, C2D_Color32(12, 24, 19, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.40F, 284.0F, 208.0F, C2D_Color32(250, 247, 238, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.45F, 7.0F, 208.0F, Error);
    C2D_DrawRectSolid(25.0F, 20.0F, 0.45F, 277.0F, 36.0F, C2D_Color32(255, 225, 214, 255));
    ui.drawText(notice.title(), 36.0F, 30.0F, 0.52F, Error);

    const bool hasSubject = !notice.subject().empty();
    if (hasSubject) {
        ui.drawText(notice.subject(), 36.0F, 68.0F, 0.62F, HeaderInk);
        ui.drawText(notice.location(), 36.0F, 90.0F, 0.38F, Muted);
    }
    const std::string_view message = notice.message().empty() ? fallbackMessage : notice.message();
    const std::vector<std::string> lines = wrapLines(message, hasSubject ? 4 : 6);
    const float top = hasSubject ? 116.0F : 76.0F;
    const float size = hasSubject ? 0.36F : 0.38F;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        ui.drawText(lines[index], 36.0F, top + static_cast<float>(index) * 15.0F, size, hasSubject ? HeaderInk : Ink);
    }

    const UiRect& ok = ErrorDialogOkButton;
    C2D_DrawRectSolid(ok.x, ok.y, 0.46F, ok.width, ok.height, Brand);
    C2D_DrawRectSolid(ok.x + 2.0F, ok.y + 2.0F, 0.47F, ok.width - 4.0F, ok.height - 4.0F, CursorGreen);
    ui.drawCentered(okLabel, 160.0F, 199.0F, 0.52F, Surface);
    ui.drawCentered("A / B", 268.0F, 201.0F, 0.30F, Muted);
}
}
