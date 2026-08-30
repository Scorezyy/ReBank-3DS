#include "gui/GfxResources.hpp"
#include "core/Logger.hpp"

#include <3ds.h>

#include <string>

void GfxResources::load() {
    gfxInitDefault();
    gfxSet3D(true);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 4);
    C2D_Prepare();
    topLeft = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuffer = C2D_TextBufNew(16384);
    textBufferTopA = C2D_TextBufNew(4096);
    textBufferTopB = C2D_TextBufNew(4096);
    u8 consoleRegion = CFG_REGION_USA;
    const bool regionKnown = R_SUCCEEDED(CFGU_SecureInfoGetRegion(&consoleRegion));
    if (regionKnown) {
        textFont = C2D_FontLoadSystem(static_cast<CFG_Region>(consoleRegion));
    }
    const CFG_Region fallbackRegions[] = {CFG_REGION_USA, CFG_REGION_EUR, CFG_REGION_JPN};
    for (const CFG_Region region : fallbackRegions) {
        if (textFont) {
            break;
        }
        textFont = C2D_FontLoadSystem(region);
    }
    Logger::instance().info(textFont
        ? "System font loaded (console region known=" + std::to_string(regionKnown)
              + ", region=" + std::to_string(consoleRegion) + ")"
        : "System font failed to load for every region, falling back to default glyphs");
    if (textFont) {
        C2D_FontSetFilter(textFont, GPU_NEAREST, GPU_LINEAR);
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
    typeBanners = C2D_SpriteSheetLoad("romfs:/assets/types.t3x");
    Logger::instance().info(typeBanners ? "Type banner sheet loaded" : "Type banner sheet load failed");
    if (typeBanners) {
        const C2D_Image typeBannerImage = C2D_SpriteSheetGetImage(typeBanners, 0);
        if (typeBannerImage.tex) {
            C3D_TexSetFilter(typeBannerImage.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }
    teamBackground = C2D_SpriteSheetLoad("romfs:/assets/team_bg.t3x");
    Logger::instance().info(teamBackground ? "Team background sheet loaded" : "Team background sheet load failed");
    if (teamBackground) {
        const C2D_Image teamBgImage = C2D_SpriteSheetGetImage(teamBackground, 0);
        if (teamBgImage.tex) {
            C3D_TexSetFilter(teamBgImage.tex, GPU_NEAREST, GPU_NEAREST);
        }
    }
    nameDexPlate = C2D_SpriteSheetLoad("romfs:/assets/name_dex_plate.t3x");
    Logger::instance().info(nameDexPlate ? "Name/dex plate sheet loaded" : "Name/dex plate sheet load failed");
    infoStripe = C2D_SpriteSheetLoad("romfs:/assets/info_stripe.t3x");
    Logger::instance().info(infoStripe ? "Info stripe sheet loaded" : "Info stripe sheet load failed");
    pointSmall = C2D_SpriteSheetLoad("romfs:/assets/point_small.t3x");
    Logger::instance().info(pointSmall ? "Point small sheet loaded" : "Point small sheet load failed");
    genderMaleIcon = C2D_SpriteSheetLoad("romfs:/assets/icon_male.t3x");
    genderFemaleIcon = C2D_SpriteSheetLoad("romfs:/assets/icon_female.t3x");
    genderlessIcon = C2D_SpriteSheetLoad("romfs:/assets/icon_genderless.t3x");
    Logger::instance().info(genderMaleIcon && genderFemaleIcon && genderlessIcon
        ? "Gender icon sheets loaded" : "Gender icon sheet load failed");
    gameSelectorCard = C2D_SpriteSheetLoad("romfs:/assets/gameselector_card.t3x");
    Logger::instance().info(gameSelectorCard ? "Game selector card sheet loaded" : "Game selector card sheet load failed");
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
    if (typeBanners) {
        C2D_SpriteSheetFree(typeBanners);
    }
    if (teamBackground) {
        C2D_SpriteSheetFree(teamBackground);
    }
    if (nameDexPlate) {
        C2D_SpriteSheetFree(nameDexPlate);
    }
    if (infoStripe) {
        C2D_SpriteSheetFree(infoStripe);
    }
    if (pointSmall) {
        C2D_SpriteSheetFree(pointSmall);
    }
    if (genderMaleIcon) {
        C2D_SpriteSheetFree(genderMaleIcon);
    }
    if (genderFemaleIcon) {
        C2D_SpriteSheetFree(genderFemaleIcon);
    }
    if (genderlessIcon) {
        C2D_SpriteSheetFree(genderlessIcon);
    }
    if (gameSelectorCard) {
        C2D_SpriteSheetFree(gameSelectorCard);
    }
    if (textFont) {
        C2D_FontFree(textFont);
    }
    C2D_TextBufDelete(textBuffer);
    C2D_TextBufDelete(textBufferTopA);
    C2D_TextBufDelete(textBufferTopB);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}
