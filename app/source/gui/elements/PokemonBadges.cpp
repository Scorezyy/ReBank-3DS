#include "gui/elements/PokemonBadges.hpp"
#include "gui/Theme.hpp"

#include <cmath>

namespace Gui {
void drawPokemonBadges(C2D_SpriteSheet overlaySheet, const PokemonSummary& pokemon,
                        float cx, float cy, float halfW, float halfH, float z) {
    if (!overlaySheet) {
        return;
    }
    if (pokemon.shiny) {
        const C2D_Image star = C2D_SpriteSheetGetImage(overlaySheet, OverlayShinyIdx);
        C2D_DrawImageAt(star,
                        std::round(cx + halfW - static_cast<float>(star.subtex->width)),
                        std::round(cy - halfH),
                        z);
    }
    if (pokemon.heldItem != 0) {
        const C2D_Image item = C2D_SpriteSheetGetImage(overlaySheet, OverlayItemIdx);
        C2D_DrawImageAt(item,
                        std::round(cx - halfW),
                        std::round(cy + halfH - static_cast<float>(item.subtex->height)),
                        z);
    }
}

namespace {
constexpr int EnglishTypesStart = 18;

C2D_Image typeBannerImage(C2D_SpriteSheet typeSheet, pksm::Type type) {
    if (!typeSheet) {
        return {};
    }
    const auto typeValue = static_cast<int>(static_cast<pksm::Type::EnumType>(type));
    if (typeValue < 0 || typeValue > 17) {
        return {};
    }
    return C2D_SpriteSheetGetImage(typeSheet, EnglishTypesStart + typeValue);
}
}

void drawTypeBanner(C2D_SpriteSheet typeSheet, pksm::Type type, float x, float y, float z, float scale) {
    const C2D_Image banner = typeBannerImage(typeSheet, type);
    if (!banner.tex) {
        return;
    }
    C2D_DrawImageAt(banner, std::round(x), std::round(y), z, nullptr, scale, scale);
}

float typeBannerHeight(C2D_SpriteSheet typeSheet, pksm::Type type) {
    const C2D_Image banner = typeBannerImage(typeSheet, type);
    return banner.subtex ? static_cast<float>(banner.subtex->height) : 0.0F;
}

float typeBannerWidth(C2D_SpriteSheet typeSheet, pksm::Type type, float scale) {
    const C2D_Image banner = typeBannerImage(typeSheet, type);
    return banner.subtex ? static_cast<float>(banner.subtex->width) * scale : 0.0F;
}
}
