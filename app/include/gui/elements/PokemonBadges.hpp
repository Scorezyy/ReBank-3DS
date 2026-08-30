#pragma once

#include "save/adapter/SaveAdapter.hpp"

#include <citro2d.h>

namespace Gui {
void drawPokemonBadges(C2D_SpriteSheet overlaySheet, const PokemonSummary& pokemon,
                        float cx, float cy, float halfW, float halfH, float z);

void drawTypeBanner(C2D_SpriteSheet typeSheet, pksm::Type type, float x, float y, float z, float scale = 1.0F);
float typeBannerHeight(C2D_SpriteSheet typeSheet, pksm::Type type);
float typeBannerWidth(C2D_SpriteSheet typeSheet, pksm::Type type, float scale = 1.0F);
}
