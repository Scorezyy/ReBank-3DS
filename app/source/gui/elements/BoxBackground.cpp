#include "gui/elements/BoxBackground.hpp"
#include "gui/Theme.hpp"

#include <3ds.h>

#include <cmath>

namespace Gui {
namespace {
Tex3DS_SubTexture selectSubRegion(const C2D_Image& image, int x, int y, int endX, int endY) {
    Tex3DS_SubTexture tex = *image.subtex;
    if (x != endX) {
        const int deltaX = endX - x;
        const float texRL = tex.left - tex.right;
        tex.left = tex.left - texRL / static_cast<float>(tex.width) * static_cast<float>(x);
        tex.right = tex.left - texRL / static_cast<float>(tex.width) * static_cast<float>(deltaX);
        tex.width = static_cast<u16>(deltaX);
    }
    if (y != endY) {
        const int deltaY = endY - y;
        const float texTB = tex.top - tex.bottom;
        tex.top = tex.top - texTB / static_cast<float>(tex.height) * static_cast<float>(y);
        tex.bottom = tex.top - texTB / static_cast<float>(tex.height) * static_cast<float>(deltaY);
        tex.height = static_cast<u16>(deltaY);
    }
    return tex;
}
}

void drawBoxBackground(C2D_SpriteSheet sheet, bool top) {
    if (!sheet) {
        return;
    }
    const C2D_Image gradient = C2D_SpriteSheetGetImage(
        sheet, top ? BoxBgTopGradientIdx : BoxBgBottomGradientIdx);
    C2D_ImageTint tint{};
    if (top) {
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(142, 221, 138, 255), 1.0F);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(101, 193, 93, 255), 1.0F);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(161, 233, 158, 255), 1.0F);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(119, 205, 113, 255), 1.0F);
    } else {
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(125, 209, 119, 255), 1.0F);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(161, 233, 158, 255), 1.0F);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(101, 193, 93, 255), 1.0F);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(136, 217, 131, 255), 1.0F);
    }
    C2D_DrawImageAt(gradient, 0.0F, 0.0F, 0.02F, &tint);

    constexpr float pixelsPerSecond = 14.0F;
    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float offset = std::fmod(static_cast<float>(seconds) * pixelsPerSecond, 400.0F);
    const float scrollA = -offset;
    const float scrollB = 400.0F - offset;

    const C2D_Image squares = C2D_SpriteSheetGetImage(sheet, BoxBgAnimSquaresIdx);
    const Tex3DS_SubTexture leftHalf = selectSubRegion(squares, 0, 0, 400, 240);
    const Tex3DS_SubTexture rightHalf = selectSubRegion(squares, 400, 0, 800, 240);
    C2D_DrawImageAt({squares.tex, &leftHalf}, scrollA, 0.0F, 0.03F);
    C2D_DrawImageAt({squares.tex, &rightHalf}, scrollB, 0.0F, 0.03F);
}

void drawLinePattern(C2D_SpriteSheet sheet, u32 baseColor, bool animated) {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 320.0F, 240.0F, baseColor);
    if (!sheet) {
        return;
    }
    const C2D_Image pattern = C2D_SpriteSheetGetImage(sheet, 0);
    if (!pattern.tex) {
        return;
    }
    float driftX = 0.0F;
    float driftY = 0.0F;
    if (animated) {
        const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
        driftX = std::sin(static_cast<float>(seconds) * 0.23F) * 6.0F;
        driftY = std::sin(static_cast<float>(seconds) * 0.17F + 1.3F) * 5.0F;
    }
    C2D_DrawImageAt(pattern, driftX, driftY, 0.01F);
}
}
