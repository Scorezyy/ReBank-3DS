#include "gui/GameVisual.hpp"
#include "save/catalog/GameCatalog.hpp"

#include <citro2d.h>

#include <algorithm>

namespace Gui {
GameVisual gameVisual(std::string_view code) {
    if (code == "x") return {"X", C2D_Color32(35, 104, 184, 255), C2D_Color32(100, 195, 240, 255)};
    if (code == "y") return {"Y", C2D_Color32(190, 42, 52, 255), C2D_Color32(245, 115, 105, 255)};
    if (code == "omega-ruby") return {"OR", C2D_Color32(190, 42, 45, 255), C2D_Color32(245, 146, 75, 255)};
    if (code == "alpha-sapphire") return {"AS", C2D_Color32(30, 95, 180, 255), C2D_Color32(82, 192, 220, 255)};
    if (code == "sun") return {"S", C2D_Color32(224, 118, 25, 255), C2D_Color32(250, 204, 60, 255)};
    if (code == "moon") return {"M", C2D_Color32(48, 72, 150, 255), C2D_Color32(114, 146, 220, 255)};
    if (code == "ultra-sun") return {"US", C2D_Color32(220, 80, 30, 255), C2D_Color32(250, 190, 45, 255)};
    if (code == "ultra-moon") return {"UM", C2D_Color32(65, 60, 145, 255), C2D_Color32(120, 120, 220, 255)};
    if (code == "diamond") return {"D", C2D_Color32(56, 145, 190, 255), C2D_Color32(150, 225, 240, 255)};
    if (code == "pearl") return {"P", C2D_Color32(190, 95, 145, 255), C2D_Color32(245, 185, 210, 255)};
    if (code == "platinum") return {"Pt", C2D_Color32(75, 82, 88, 255), C2D_Color32(184, 191, 195, 255)};
    if (code == "heartgold") return {"HG", C2D_Color32(181, 125, 28, 255), C2D_Color32(245, 210, 90, 255)};
    if (code == "soulsilver") return {"SS", C2D_Color32(86, 112, 128, 255), C2D_Color32(190, 215, 224, 255)};
    if (code == "black" || code == "black2") {
        return {code == "black2" ? "B2" : "B", C2D_Color32(35, 39, 42, 255), C2D_Color32(120, 128, 132, 255)};
    }
    return {code == "white2" ? "W2" : "W", C2D_Color32(175, 180, 184, 255), C2D_Color32(245, 247, 248, 255)};
}

std::uint8_t pokemonFormatFromCode(const std::string& code) {
    if (code.empty()) {
        return 0;
    }
    for (const auto& game : supportedGames()) {
        if (game.code == code) {
            return static_cast<std::uint8_t>(game.format);
        }
    }
    return 0;
}

std::string paddedTrainerId(std::uint32_t trainerId) {
    if (trainerId > 99999) {
        return std::to_string(trainerId);
    }
    std::string value = std::to_string(trainerId);
    return std::string(5 - std::min<std::size_t>(5, value.size()), '0') + value;
}
}
