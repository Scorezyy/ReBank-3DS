#include "gui/screens/bank/PokemonSprites.hpp"

#include "gui/widgets/PokemonBadges.hpp"

#include <cmath>

using namespace Gui;

namespace {
constexpr u32 DimTint = C2D_Color32(72, 72, 72, 255);
constexpr u32 ShadowColor = C2D_Color32(20, 20, 20, 90);
constexpr float CarryLift = 8.0F;
}

std::optional<C2D_Image> PokemonSprites::spriteOf(const PokemonSummary& summary) const {
    if (!summary.occupied() || !resources_.pokemonSprites
        || summary.species >= C2D_SpriteSheetCount(resources_.pokemonSprites)) {
        return std::nullopt;
    }
    return C2D_SpriteSheetGetImage(resources_.pokemonSprites, summary.species);
}

void PokemonSprites::draw(const PokemonSummary& summary, float cx, float cy, float z, float badgeZ, bool dimmed,
                          float xShift) const {
    const std::optional<C2D_Image> sprite = spriteOf(summary);
    if (!sprite) {
        return;
    }
    const float width = sprite->subtex->width;
    const float height = sprite->subtex->height;
    C2D_ImageTint tint{};
    if (dimmed) {
        C2D_PlainImageTint(&tint, DimTint, 0.82F);
    }
    C2D_DrawImageAt(*sprite, std::round(cx - width * 0.5F + xShift), std::round(cy - height * 0.5F), z,
                    dimmed ? &tint : nullptr);
    drawPokemonBadges(resources_.overlayIcons, summary, cx, cy, width * 0.5F, height * 0.5F, badgeZ);
}

void PokemonSprites::drawCarried(const PokemonSummary& summary, float cx, float cy, float z) const {
    const std::optional<C2D_Image> sprite = spriteOf(summary);
    if (!sprite) {
        return;
    }
    const float width = sprite->subtex->width;
    const float height = sprite->subtex->height;
    const float shadowWidth = width * 0.6F;
    const float shadowHeight = shadowWidth * 0.35F;
    C2D_DrawEllipseSolid(cx - shadowWidth * 0.5F, cy + height * 0.42F - shadowHeight * 0.5F, z - 0.02F, shadowWidth,
                         shadowHeight, ShadowColor);
    const float spriteY = cy - CarryLift;
    C2D_DrawImageAt(*sprite, std::round(cx - width * 0.5F), std::round(spriteY - height * 0.5F), z);
    drawPokemonBadges(resources_.overlayIcons, summary, cx, spriteY, width * 0.5F, height * 0.5F, z + 0.01F);
}
