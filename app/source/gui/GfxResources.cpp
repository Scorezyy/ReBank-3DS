#include "gui/GfxResources.hpp"
#include "core/Logger.hpp"

#include <3ds.h>

#include <optional>
#include <string>

namespace {
struct SheetAsset {
    C2D_SpriteSheet GfxResources::* sheet;
    const char* path;
    std::optional<GPU_TEXTURE_FILTER_PARAM> filter;
};

constexpr SheetAsset SheetAssets[] = {
    {&GfxResources::pokemonSprites, "romfs:/assets/pkm_spritesheet.t3x", GPU_NEAREST},
    {&GfxResources::boxBackground, "romfs:/assets/box_bg.t3x", std::nullopt},
    {&GfxResources::bottomBackground, "romfs:/assets/bottom_bg.t3x", std::nullopt},
    {&GfxResources::overlayIcons, "romfs:/assets/overlay_icons.t3x", std::nullopt},
    {&GfxResources::boxNameBarSheet, "romfs:/assets/bar_boxname_with_arrows.t3x", std::nullopt},
    {&GfxResources::typeBanners, "romfs:/assets/types.t3x", GPU_LINEAR},
    {&GfxResources::teamBackground, "romfs:/assets/team_bg.t3x", GPU_NEAREST},
    {&GfxResources::nameDexPlate, "romfs:/assets/name_dex_plate.t3x", std::nullopt},
    {&GfxResources::infoStripe, "romfs:/assets/info_stripe.t3x", std::nullopt},
    {&GfxResources::pointSmall, "romfs:/assets/point_small.t3x", std::nullopt},
    {&GfxResources::genderMaleIcon, "romfs:/assets/icon_male.t3x", std::nullopt},
    {&GfxResources::genderFemaleIcon, "romfs:/assets/icon_female.t3x", std::nullopt},
    {&GfxResources::gameSelectorCard, "romfs:/assets/gameselector_card.t3x", std::nullopt},
};
}

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
    loadFont();

    for (const SheetAsset& asset : SheetAssets) {
        C2D_SpriteSheet& sheet = this->*asset.sheet;
        sheet = C2D_SpriteSheetLoad(asset.path);
        if (!sheet) {
            Logger::instance().error(std::string("Sprite sheet could not be loaded: ") + asset.path);
            continue;
        }
        const C2D_Image image = C2D_SpriteSheetGetImage(sheet, 0);
        if (asset.filter && image.tex) {
            C3D_TexSetFilter(image.tex, *asset.filter, *asset.filter);
        }
    }
}

void GfxResources::loadFont() {
    u8 consoleRegion = CFG_REGION_USA;
    const bool regionKnown = R_SUCCEEDED(CFGU_SecureInfoGetRegion(&consoleRegion));
    if (regionKnown) {
        textFont = C2D_FontLoadSystem(static_cast<CFG_Region>(consoleRegion));
    }
    for (const CFG_Region region : {CFG_REGION_USA, CFG_REGION_EUR, CFG_REGION_JPN}) {
        if (!textFont) {
            textFont = C2D_FontLoadSystem(region);
        }
    }
    if (!textFont) {
        Logger::instance().warning("System font failed to load for every region, falling back to default glyphs");
        return;
    }
    C2D_FontSetFilter(textFont, GPU_NEAREST, GPU_LINEAR);
    Logger::instance().info("System font loaded (console region known=" + std::to_string(regionKnown)
                            + ", region=" + std::to_string(consoleRegion) + ")");
}

GfxResources::~GfxResources() {
    for (const SheetAsset& asset : SheetAssets) {
        if (C2D_SpriteSheet sheet = this->*asset.sheet) {
            C2D_SpriteSheetFree(sheet);
        }
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
