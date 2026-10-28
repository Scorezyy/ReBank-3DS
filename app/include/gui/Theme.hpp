#pragma once

#include <citro2d.h>

struct UiRect {
    float x;
    float y;
    float width;
    float height;

    bool contains(touchPosition point) const;
};

namespace Gui {
inline constexpr u32 Background = C2D_Color32(238, 248, 240, 255);
inline constexpr u32 Surface = C2D_Color32(255, 255, 255, 255);
inline constexpr u32 Ink = C2D_Color32(20, 43, 34, 255);
inline constexpr u32 Muted = C2D_Color32(86, 111, 99, 255);
inline constexpr u32 Brand = C2D_Color32(31, 145, 94, 255);
inline constexpr u32 Accent = C2D_Color32(242, 184, 39, 255);
inline constexpr u32 Error = C2D_Color32(190, 48, 48, 255);
inline constexpr u32 WarmBackground = C2D_Color32(255, 244, 232, 255);
inline constexpr u32 WarmGlowTop = C2D_Color32(240, 138, 60, 255);
inline constexpr u32 WarmGlowBottom = C2D_Color32(184, 58, 12, 255);
inline constexpr u32 WarmBrand = C2D_Color32(232, 105, 42, 255);
inline constexpr u32 WarmBrandDark = C2D_Color32(154, 52, 18, 255);
inline constexpr u32 WarmInk = C2D_Color32(84, 36, 14, 255);
inline constexpr u32 WarmMuted = C2D_Color32(160, 104, 74, 255);
inline constexpr u32 WarmCream = C2D_Color32(255, 226, 184, 255);
inline constexpr int LogoTopIdx = 0;
inline constexpr int LogoSmallIdx = 1;
inline constexpr int BoxBgAnimSquaresIdx = 0;
inline constexpr int OverlayItemIdx = 0;
inline constexpr int OverlayShinyIdx = 1;
inline constexpr int BoxBgTopGradientIdx = 1;
inline constexpr u32 HeaderPill = C2D_Color32(246, 244, 224, 250);
inline constexpr u32 HeaderInk = C2D_Color32(64, 66, 40, 255);
inline constexpr u32 CountBlock = C2D_Color32(174, 44, 44, 255);
inline constexpr u32 BoxPlate = C2D_Color32(250, 252, 248, 250);
inline constexpr u32 BoxPlateBorder = C2D_Color32(20, 110, 70, 255);
inline constexpr u32 BoxArrowInk = C2D_Color32(20, 110, 70, 255);
inline constexpr u32 CursorRed = C2D_Color32(216, 40, 32, 255);
inline constexpr u32 CursorGreen = C2D_Color32(40, 176, 88, 255);
inline constexpr u32 CursorBlue = C2D_Color32(40, 120, 216, 255);
inline constexpr u32 SelectionHighlightFill = C2D_Color32(242, 184, 39, 150);
inline constexpr float SelectionHighlightDepth = 0.90F;
inline constexpr float HeldRegionSpriteDepth = 0.95F;
inline constexpr UiRect SubmitButton{24.0F, 195.0F, 272.0F, 38.0F};
inline constexpr UiRect BackButton{8.0F, 8.0F, 70.0F, 32.0F};
inline constexpr UiRect LogoutButton{2.0F, 199.0F, 66.0F, 32.0F};
inline constexpr UiRect PluginButton{2.0F, 167.0F, 66.0F, 30.0F};
}
