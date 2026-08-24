#pragma once

#include "save/SaveAdapter.hpp"

#include <citro2d.h>

namespace Gui {
// Shiny star / held-item corner badges overlaid on a box-grid sprite.
void drawPokemonBadges(C2D_SpriteSheet overlaySheet, const PokemonSummary& pokemon,
                        float cx, float cy, float halfW, float halfH, float z);
}
