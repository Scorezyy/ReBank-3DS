#pragma once

#include <citro2d.h>

// Owns every citro2d/citro3d rendering resource the app loads once and
// keeps for its whole lifetime: the three screen render targets, the
// shared text buffer/font, and every sprite sheet.
//
// load() must be called after romfs is mounted (sprite sheets live in
// romfs:/assets/...); the constructor itself does no graphics or file work,
// so building this member never races that ordering requirement.
class GfxResources {
public:
    GfxResources() = default;
    ~GfxResources();

    GfxResources(const GfxResources&) = delete;
    GfxResources& operator=(const GfxResources&) = delete;

    void load();

    C3D_RenderTarget* topLeft = nullptr;
    C3D_RenderTarget* topRight = nullptr;
    C3D_RenderTarget* bottom = nullptr;
    C2D_TextBuf textBuffer = nullptr;
    C2D_Font textFont = nullptr;
    C2D_SpriteSheet pokemonSprites = nullptr;
    C2D_SpriteSheet boxBackground = nullptr;
    C2D_SpriteSheet overlayIcons = nullptr;
    C2D_SpriteSheet iconItemSheet = nullptr;
    C2D_SpriteSheet iconShinySheet = nullptr;
    C2D_SpriteSheet boxNameBarSheet = nullptr;
};
