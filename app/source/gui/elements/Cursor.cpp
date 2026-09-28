#include "gui/elements/Cursor.hpp"

#include <3ds.h>

#include <cmath>

namespace Gui {
namespace {
constexpr int ArrowSteps = 6;

void drawDownArrow(float cx, float topY, float size, u32 color) {
    const float stepHeight = size / ArrowSteps;
    for (int step = 0; step < ArrowSteps; ++step) {
        const float width = size * (1.0F - static_cast<float>(step) / ArrowSteps);
        C2D_DrawRectSolid(cx - width * 0.5F, topY + step * (stepHeight + 0.5F), 0.97F, width, stepHeight + 1.0F, color);
    }
}
}

void drawBouncingCursor(float cx, float baseTopY, float amplitude, float size, u32 color) {
    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    drawDownArrow(cx, baseTopY + std::sin(static_cast<float>(seconds) * 6.0F) * amplitude, size, color);
}
}
