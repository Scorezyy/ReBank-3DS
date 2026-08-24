#pragma once

#include <citro2d.h>

namespace Gui {
// Draws a single down-pointing arrow, tapered top to bottom.
void drawDownArrow(float cx, float topY, float size, u32 color);

// A down arrow that bobs up and down over time - the focus indicator used
// throughout the box grids and box-name field.
void drawBouncingCursor(float cx, float baseTopY, float amplitude, float size, u32 color);
}
