#include "gui/elements/AuthFormChrome.hpp"

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
}
