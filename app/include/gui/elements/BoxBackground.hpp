#pragma once

#include <citro2d.h>

namespace Gui {
void drawBoxBackground(C2D_SpriteSheet sheet, bool top, float trashProgress = 0.0F);

void drawLinePattern(C2D_SpriteSheet sheet, u32 baseColor, bool animated);
}
