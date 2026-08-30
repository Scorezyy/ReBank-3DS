#include "gui/elements/AuthFormChrome.hpp"
#include "gui/UiRenderer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace Gui {
void drawAuthFormBackdrop(double animationSeconds) {
    for (int i = 0; i < 3; ++i) {
        const float bandY = 40.0F + i * 62.0F
            + std::sin(static_cast<float>(animationSeconds) + i) * 2.0F;
        C2D_DrawRectSolid(0.0F, bandY, 0.02F, 320.0F, 2.0F, C2D_Color32(210, 220, 240, 60));
    }
}

void drawFocusRing(const UiRect& rect, float pulse) {
    const u8 alpha = static_cast<u8>(140 + pulse * 100);
    const u32 ring = C2D_Color32(70, 132, 200, alpha);
    C2D_DrawRectSolid(rect.x - 3.0F, rect.y - 3.0F, 0.08F, rect.width + 6.0F, 3.0F, ring);
    C2D_DrawRectSolid(rect.x - 3.0F, rect.y + rect.height, 0.08F, rect.width + 6.0F, 3.0F, ring);
    C2D_DrawRectSolid(rect.x - 3.0F, rect.y - 3.0F, 0.08F, 3.0F, rect.height + 6.0F, ring);
    C2D_DrawRectSolid(rect.x + rect.width, rect.y - 3.0F, 0.08F, 3.0F, rect.height + 6.0F, ring);
}

bool isValidUsername(const std::string& value) {
    return value.size() >= 3 && value.size() <= 32
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isalnum(character) || character == '_' || character == '-';
        });
}

void drawAutoLoginCheckbox(UiRenderer& ui, const UiRect& rect, bool autoLogin) {
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, C2D_Color32(250, 250, 254, 220));
    const float boxX = rect.x + 8.0F;
    const float boxY = rect.y + 5.0F;
    C2D_DrawRectSolid(boxX, boxY, 0.12F, 16.0F, 16.0F,
                      autoLogin ? C2D_Color32(40, 176, 88, 255) : C2D_Color32(210, 214, 224, 255));
    C2D_DrawRectSolid(boxX + 2.0F, boxY + 2.0F, 0.13F, 12.0F, 12.0F,
                      autoLogin ? C2D_Color32(80, 200, 120, 255) : C2D_Color32(240, 244, 252, 255));
    if (autoLogin) {
        C2D_DrawRectSolid(boxX + 4.0F, boxY + 8.0F, 0.14F, 3.0F, 4.0F, C2D_Color32(255, 255, 255, 255));
        C2D_DrawRectSolid(boxX + 5.0F, boxY + 10.0F, 0.14F, 8.0F, 3.0F, C2D_Color32(255, 255, 255, 255));
    }
    ui.drawText("Auto-Login (Y)", boxX + 26.0F, rect.y + 7.0F, 0.44F, Ink);
}

void drawSubmitTypingDots(double animationSeconds) {
    for (int i = 0; i < 3; ++i) {
        const float phase = static_cast<float>(animationSeconds) * 4.0F + i * 0.6F;
        const float px = 140.0F + i * 14.0F;
        const float py = 226.0F - std::abs(std::sin(phase)) * 6.0F;
        C2D_DrawCircleSolid(px, py, 0.4F, 4.0F, C2D_Color32(70, 132, 200, 255));
    }
}
}
