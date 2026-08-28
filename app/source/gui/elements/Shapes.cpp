#include "gui/elements/Shapes.hpp"

#include <algorithm>

namespace Gui {
void drawPill(float x, float y, float w, float h, float z, u32 color) {
    const float r = h * 0.5F;
    C2D_DrawCircleSolid(x + r, y + r, z, r, color);
    C2D_DrawCircleSolid(x + w - r, y + r, z, r, color);
    C2D_DrawRectSolid(x + r, y, z, w - 2.0F * r, h, color);
}

void drawRoundedRect(float x, float y, float w, float h, float radius, float z, u32 color) {
    const float r = std::min({radius, w * 0.5F, h * 0.5F});
    C2D_DrawCircleSolid(x + r, y + r, z, r, color);
    C2D_DrawCircleSolid(x + w - r, y + r, z, r, color);
    C2D_DrawCircleSolid(x + r, y + h - r, z, r, color);
    C2D_DrawCircleSolid(x + w - r, y + h - r, z, r, color);
    C2D_DrawRectSolid(x + r, y, z, w - 2.0F * r, h, color);
    C2D_DrawRectSolid(x, y + r, z, w, h - 2.0F * r, color);
}

void drawPlusMark(float cx, float cy, u32 color) {
    C2D_DrawRectSolid(cx - 4.0F, cy - 0.5F, 0.12F, 8.0F, 1.5F, color);
    C2D_DrawRectSolid(cx - 0.5F, cy - 4.0F, 0.12F, 1.5F, 8.0F, color);
}
}
