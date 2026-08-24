#include "gui/elements/Cursor.hpp"

#include <3ds.h>

#include <cmath>

namespace Gui {
void drawDownArrow(float cx, float topY, float size, u32 color) {
    for (int i = 0; i < 6; ++i) {
        const float w = size * (1.0F - static_cast<float>(i) / 6.0F);
        C2D_DrawRectSolid(cx - w * 0.5F, topY + i * (size / 6.0F + 0.5F), 0.5F,
                          w, size / 6.0F + 1.0F, color);
    }
}

void drawBouncingCursor(float cx, float baseTopY, float amplitude, float size, u32 color) {
    const double t = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float bounce = std::sin(static_cast<float>(t) * 6.0F) * amplitude;
    drawDownArrow(cx, baseTopY + bounce, size, color);
}
}
