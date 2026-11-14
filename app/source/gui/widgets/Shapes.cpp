#include "gui/widgets/Shapes.hpp"

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

void drawRoundedRect(const UiRect& rect, float radius, float z, u32 color) {
    drawRoundedRect(rect.x, rect.y, rect.width, rect.height, radius, z, color);
}

void drawCard(const UiRect& rect, float radius, float border, u32 borderColor, u32 fill) {
    drawRoundedRect(rect, radius, 0.1F, borderColor);
    drawRoundedRect({rect.x + border, rect.y + border, rect.width - border * 2.0F, rect.height - border * 2.0F},
                    radius - border, 0.11F, fill);
}

void drawStretched(C2D_SpriteSheet sheet, float x, float y, float z, float width, float height) {
    const C2D_Image image = C2D_SpriteSheetGetImage(sheet, 0);
    C2D_DrawImageAt(image, x, y, z, nullptr, width / static_cast<float>(image.subtex->width),
                    height / static_cast<float>(image.subtex->height));
}

void drawPlusMark(float cx, float cy, u32 color) {
    C2D_DrawRectSolid(cx - 4.0F, cy - 0.5F, 0.12F, 8.0F, 1.5F, color);
    C2D_DrawRectSolid(cx - 0.5F, cy - 4.0F, 0.12F, 1.5F, 8.0F, color);
}
}
