#pragma once

#include "gui/GfxResources.hpp"
#include "save/pokemon/PokemonData.hpp"

#include <citro2d.h>

#include <optional>

class PokemonSprites {
public:
    explicit PokemonSprites(const GfxResources& resources) : resources_(resources) {}

    void draw(const PokemonSummary& summary, float cx, float cy, float z, float badgeZ, bool dimmed,
              float xShift = 0.0F) const;
    void drawCarried(const PokemonSummary& summary, float cx, float cy, float z) const;

private:
    std::optional<C2D_Image> spriteOf(const PokemonSummary& summary) const;

    const GfxResources& resources_;
};
