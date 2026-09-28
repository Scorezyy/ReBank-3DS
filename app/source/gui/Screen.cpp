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
    const float drift = std::sin(static_cast<float>(seconds) * 1.7F) * 4.0F;
    C2D_DrawCircleSolid(200.0F + eyeOffset + drift, 91.0F, 0.0F, 62.0F, Brand);
    C2D_DrawCircleSolid(200.0F + eyeOffset - drift * 0.4F, 91.0F, 0.0F, 43.0F, Accent);
    ui().drawCentered("ReBank", 200.0F + eyeOffset, 71.0F, 1.7F, Ink);
    ui().drawCentered(text(TextId::Tagline), 200.0F, 171.0F, 0.62F, Muted);
}
