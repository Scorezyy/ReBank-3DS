#pragma once

#include <citro2d.h>

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
    C2D_TextBuf textBufferTopA = nullptr;
    C2D_TextBuf textBufferTopB = nullptr;
    C2D_TextBuf textBuffer = nullptr;
    C2D_Font textFont = nullptr;
    C2D_SpriteSheet pokemonSprites = nullptr;
    C2D_SpriteSheet boxBackground = nullptr;
    C2D_SpriteSheet bottomBackground = nullptr;
    C2D_SpriteSheet overlayIcons = nullptr;
    C2D_SpriteSheet boxNameBarSheet = nullptr;
    C2D_SpriteSheet typeBanners = nullptr;
    C2D_SpriteSheet teamBackground = nullptr;
    C2D_SpriteSheet nameDexPlate = nullptr;
    C2D_SpriteSheet infoStripe = nullptr;
    C2D_SpriteSheet pointSmall = nullptr;
    C2D_SpriteSheet genderMaleIcon = nullptr;
    C2D_SpriteSheet genderFemaleIcon = nullptr;
    C2D_SpriteSheet gameSelectorCard = nullptr;

private:
    void loadFont();
};
