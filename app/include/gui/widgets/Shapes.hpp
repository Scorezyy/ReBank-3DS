#pragma once

#include "gui/Theme.hpp"

#include <citro2d.h>

namespace Gui {
void drawPill(float x, float y, float w, float h, float z, u32 color);
void drawRoundedRect(float x, float y, float w, float h, float radius, float z, u32 color);
void drawRoundedRect(const UiRect& rect, float radius, float z, u32 color);
void drawCard(const UiRect& rect, float radius, float border, u32 borderColor, u32 fill);
void drawStretched(C2D_SpriteSheet sheet, float x, float y, float z, float width, float height);
void drawPlusMark(float cx, float cy, u32 color);
}
