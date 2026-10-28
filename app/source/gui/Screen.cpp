#include "gui/Screen.hpp"
#include "app/App.hpp"

#include <cmath>

using namespace Gui;

UiRenderer& Screen::ui() const {
    return app_.ui_;
}

std::string_view Screen::text(TextId id) const {
    return app_.localization_.get(id);
}

std::string Screen::format(TextId id, std::initializer_list<std::string_view> arguments) const {
    return app_.localization_.format(id, arguments);
}

void Screen::renderTop(float eyeOffset) {
    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float bob = std::sin(static_cast<float>(seconds) * 1.4F) * 3.0F;
    ui().drawWarmTop(eyeOffset);
    if (app_.resources_.logo) {
        const C2D_Image logo = C2D_SpriteSheetGetImage(app_.resources_.logo, LogoTopIdx);
        const float width = logo.subtex->width;
        C2D_DrawImageAt(logo, 200.0F + eyeOffset - width * 0.5F, 10.0F + bob, 0.5F);
    } else {
        ui().drawCentered("ReBank", 200.0F + eyeOffset, 80.0F, 1.7F, Surface);
    }
    C2D_DrawRectSolid(150.0F, 202.0F, 0.4F, 100.0F, 1.0F, C2D_Color32(255, 226, 184, 140));
    ui().drawCentered(text(TextId::Tagline), 200.0F, 210.0F, 0.58F, WarmCream);
}
