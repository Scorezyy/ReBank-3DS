#pragma once

#include "gui/Theme.hpp"

#include <citro2d.h>

namespace Gui {
// Shared decoration for the Login/Register/Reset-password forms: the
// slowly drifting background bands and the pulsing ring drawn around
// whichever field currently has focus.
void drawAuthFormBackdrop(double animationSeconds);
void drawFocusRing(const UiRect& rect, float pulse);
}
