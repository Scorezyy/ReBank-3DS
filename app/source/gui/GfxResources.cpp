#include "gui/GfxResources.hpp"
#include "core/Logger.hpp"

void GfxResources::load() {
    gfxInitDefault();
    gfxSet3D(true);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topLeft = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuffer = C2D_TextBufNew(4096);
    textFont = C2D_FontLoadSystem(CFG_REGION_JPN);
    if (!textFont) {
        Logger::instance().warning("Japanese system font could not be loaded");
    }
    pokemonSprites = C2D_SpriteSheetLoad("romfs:/assets/pkm_spritesheet.t3x");
    if (!pokemonSprites) {
        Logger::instance().error("Pokemon sprite sheet could not be loaded");
    } else {
        const C2D_Image sheetImage = C2D_SpriteSheetGetImage(pokemonSprites, 0);
        if (sheetImage.tex) {
            C3D_TexSetFilter(sheetImage.tex, GPU_NEAREST, GPU_NEAREST);
        }
    }
    boxBackground = C2D_SpriteSheetLoad("romfs:/assets/box_bg.t3x");
    if (!boxBackground) {
        Logger::instance().error("Box background sheet could not be loaded");
    }
    bottomBackground = C2D_SpriteSheetLoad("romfs:/assets/bottom_bg.t3x");
    if (!bottomBackground) {
        Logger::instance().error("Bottom background sheet could not be loaded");
    }
    overlayIcons = C2D_SpriteSheetLoad("romfs:/assets/overlay_icons.t3x");
    if (!overlayIcons) {
        Logger::instance().error("Overlay icon sheet could not be loaded");
    }
    iconItemSheet = C2D_SpriteSheetLoad("romfs:/assets/icon_item.t3x");
    Logger::instance().info(iconItemSheet ? "Item icon sheet loaded" : "Item icon sheet load failed");
    iconShinySheet = C2D_SpriteSheetLoad("romfs:/assets/icon_shiny.t3x");
    Logger::instance().info(iconShinySheet ? "Shiny icon sheet loaded" : "Shiny icon sheet load failed");
    boxNameBarSheet = C2D_SpriteSheetLoad("romfs:/assets/bar_boxname_with_arrows.t3x");
    Logger::instance().info(boxNameBarSheet ? "Box name bar sheet loaded" : "Box name bar sheet load failed");
}

GfxResources::~GfxResources() {
    if (pokemonSprites) {
        C2D_SpriteSheetFree(pokemonSprites);
    }
    if (boxBackground) {
        C2D_SpriteSheetFree(boxBackground);
    }
    if (bottomBackground) {
        C2D_SpriteSheetFree(bottomBackground);
    }
    if (overlayIcons) {
        C2D_SpriteSheetFree(overlayIcons);
    }
    if (iconItemSheet) {
        C2D_SpriteSheetFree(iconItemSheet);
    }
    if (iconShinySheet) {
        C2D_SpriteSheetFree(iconShinySheet);
    }
    if (boxNameBarSheet) {
        C2D_SpriteSheetFree(boxNameBarSheet);
    }
    if (textFont) {
        C2D_FontFree(textFont);
    }
    C2D_TextBufDelete(textBuffer);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}
