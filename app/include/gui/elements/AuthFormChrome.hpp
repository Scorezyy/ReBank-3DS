#pragma once

#include "gui/Theme.hpp"

#include <citro2d.h>

#include <string>

class UiRenderer;

namespace Gui {
// Shared decoration for the Login/Register/Reset-password forms: the
// slowly drifting background bands and the pulsing ring drawn around
// whichever field currently has focus.
void drawAuthFormBackdrop(double animationSeconds);
void drawFocusRing(const UiRect& rect, float pulse);

bool isValidUsername(const std::string& value);
void drawAutoLoginCheckbox(UiRenderer& ui, const UiRect& rect, bool autoLogin);
void drawSubmitTypingDots(double animationSeconds);
}
