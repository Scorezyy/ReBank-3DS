#include "gui/elements/BoxBackground.hpp"
#include "gui/Theme.hpp"

#include <3ds.h>

#include <algorithm>
#include <cmath>

namespace Gui {
namespace {
constexpr float ScreenWidth = 400.0F;
constexpr float ScrollPixelsPerSecond = 14.0F;
constexpr u32 TrashRed = C2D_Color32(250, 128, 114, 255);

Tex3DS_SubTexture horizontalSlice(const C2D_Image& image, int startX, int width) {
    Tex3DS_SubTexture slice = *image.subtex;
    const float texelWidth = (slice.left - slice.right) / static_cast<float>(slice.width);
    slice.left -= texelWidth * static_cast<float>(startX);
    slice.right = slice.left - texelWidth * static_cast<float>(width);
    slice.width = static_cast<u16>(width);
    return slice;
}

u32 lerpColor(u32 from, u32 to, float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    const auto mix = [&](int shift) {
        const float a = static_cast<float>((from >> shift) & 0xFF);
        const float b = static_cast<float>((to >> shift) & 0xFF);
        return static_cast<u8>(a + (b - a) * t);
    };
    return C2D_Color32(mix(0), mix(8), mix(16), 255);
}
}

void drawBoxBackground(C2D_SpriteSheet sheet, float trashProgress) {
    if (!sheet) {
        return;
    }
    const std::pair<C2D_Corner, u32> corners[] = {
        {C2D_TopLeft, C2D_Color32(142, 221, 138, 255)},
        {C2D_TopRight, C2D_Color32(101, 193, 93, 255)},
        {C2D_BotLeft, C2D_Color32(161, 233, 158, 255)},
        {C2D_BotRight, C2D_Color32(119, 205, 113, 255)},
    };
    C2D_ImageTint tint{};
    for (const auto& [corner, color] : corners) {
        C2D_SetImageTint(&tint, corner, lerpColor(color, TrashRed, trashProgress), 1.0F);
    }
    C2D_DrawImageAt(C2D_SpriteSheetGetImage(sheet, BoxBgTopGradientIdx), 0.0F, 0.0F, 0.02F, &tint);

    const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const float offset = std::fmod(static_cast<float>(seconds) * ScrollPixelsPerSecond, ScreenWidth);
    const C2D_Image squares = C2D_SpriteSheetGetImage(sheet, BoxBgAnimSquaresIdx);
    const auto width = static_cast<int>(ScreenWidth);
    const Tex3DS_SubTexture leftHalf = horizontalSlice(squares, 0, width);
    const Tex3DS_SubTexture rightHalf = horizontalSlice(squares, width, width);
    C2D_DrawImageAt({squares.tex, &leftHalf}, -offset, 0.0F, 0.03F);
    C2D_DrawImageAt({squares.tex, &rightHalf}, ScreenWidth - offset, 0.0F, 0.03F);
}

void drawLinePattern(C2D_SpriteSheet sheet, u32 baseColor) {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 320.0F, 240.0F, baseColor);
    if (!sheet) {
        return;
    }
    const C2D_Image pattern = C2D_SpriteSheetGetImage(sheet, 0);
    if (pattern.tex) {
        C2D_DrawImageAt(pattern, 0.0F, 0.0F, 0.01F);
    }
}
}
