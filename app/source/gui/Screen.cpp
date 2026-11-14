#include "gui/Screen.hpp"

#include "platform/Clock.hpp"

#include <cmath>

using namespace Gui;

void Screen::drawLogo(float eyeOffset, float y, float scale) const {
    if (!app_.resources.logo) {
        return;
    }
    const C2D_Image logo = C2D_SpriteSheetGetImage(app_.resources.logo, LogoTopIdx);
    const float width = static_cast<float>(logo.subtex->width) * scale;
    C2D_DrawImageAt(logo, 200.0F + eyeOffset - width * 0.5F, y, 0.5F, nullptr, scale, scale);
}

void Screen::renderTop(float eyeOffset) {
    const double seconds = Clock::seconds();
    const float bob = std::sin(static_cast<float>(seconds) * 1.4F) * 3.0F;
    ui().drawWarmTop(eyeOffset);
    if (app_.resources.logo) {
        drawLogo(eyeOffset, 10.0F + bob, 1.0F);
    } else {
        ui().drawCentered("ReBank", 200.0F + eyeOffset, 80.0F, 1.7F, Surface);
    }
    C2D_DrawRectSolid(150.0F, 202.0F, 0.4F, 100.0F, 1.0F, C2D_Color32(255, 226, 184, 140));
    ui().drawCentered(text(TextId::Tagline), 200.0F, 210.0F, 0.58F, WarmCream);
}
