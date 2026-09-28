#include "gui/GameVisual.hpp"

#include <citro2d.h>

#include <algorithm>
#include <array>

namespace Gui {
namespace {
struct GameColors {
    std::string_view code;
    GameVisual visual;
};

constexpr GameVisual WhiteVersion{C2D_Color32(175, 180, 184, 255), C2D_Color32(245, 247, 248, 255)};
constexpr GameVisual BlackVersion{C2D_Color32(35, 39, 42, 255), C2D_Color32(120, 128, 132, 255)};

constexpr std::array Palette{
    GameColors{"x", {C2D_Color32(35, 104, 184, 255), C2D_Color32(100, 195, 240, 255)}},
    GameColors{"y", {C2D_Color32(190, 42, 52, 255), C2D_Color32(245, 115, 105, 255)}},
    GameColors{"omega-ruby", {C2D_Color32(190, 42, 45, 255), C2D_Color32(245, 146, 75, 255)}},
    GameColors{"alpha-sapphire", {C2D_Color32(30, 95, 180, 255), C2D_Color32(82, 192, 220, 255)}},
    GameColors{"sun", {C2D_Color32(224, 118, 25, 255), C2D_Color32(250, 204, 60, 255)}},
    GameColors{"moon", {C2D_Color32(48, 72, 150, 255), C2D_Color32(114, 146, 220, 255)}},
    GameColors{"ultra-sun", {C2D_Color32(220, 80, 30, 255), C2D_Color32(250, 190, 45, 255)}},
    GameColors{"ultra-moon", {C2D_Color32(65, 60, 145, 255), C2D_Color32(120, 120, 220, 255)}},
    GameColors{"diamond", {C2D_Color32(56, 145, 190, 255), C2D_Color32(150, 225, 240, 255)}},
    GameColors{"pearl", {C2D_Color32(190, 95, 145, 255), C2D_Color32(245, 185, 210, 255)}},
    GameColors{"platinum", {C2D_Color32(75, 82, 88, 255), C2D_Color32(184, 191, 195, 255)}},
    GameColors{"heartgold", {C2D_Color32(181, 125, 28, 255), C2D_Color32(245, 210, 90, 255)}},
    GameColors{"soulsilver", {C2D_Color32(86, 112, 128, 255), C2D_Color32(190, 215, 224, 255)}},
    GameColors{"black", BlackVersion},
    GameColors{"black2", BlackVersion},
};
}

GameVisual gameVisual(std::string_view code) {
    const auto match = std::find_if(Palette.begin(), Palette.end(),
        [&](const GameColors& entry) { return entry.code == code; });
    return match == Palette.end() ? WhiteVersion : match->visual;
}
}
