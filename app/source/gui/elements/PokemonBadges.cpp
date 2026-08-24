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
}
