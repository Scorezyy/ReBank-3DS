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
inline constexpr int BoxBgAnimSquaresIdx = 0;
inline constexpr int OverlayItemIdx = 0;
inline constexpr int OverlayShinyIdx = 1;
inline constexpr int BoxBgTopGradientIdx = 1;
inline constexpr int BoxBgBottomGradientIdx = 2;
inline constexpr u32 HeaderPill = C2D_Color32(246, 244, 224, 250);
inline constexpr u32 HeaderInk = C2D_Color32(64, 66, 40, 255);
inline constexpr u32 CountBlock = C2D_Color32(174, 44, 44, 255);
inline constexpr u32 BoxPlate = C2D_Color32(250, 252, 248, 250);
inline constexpr u32 BoxPlateBorder = C2D_Color32(20, 110, 70, 255);
inline constexpr u32 BoxArrowInk = C2D_Color32(20, 110, 70, 255);
inline constexpr u32 CursorRed = C2D_Color32(216, 40, 32, 255);
inline constexpr u32 CursorGreen = C2D_Color32(40, 176, 88, 255);
inline constexpr UiRect EmailField{24.0F, 62.0F, 272.0F, 42.0F};
inline constexpr UiRect PasswordField{24.0F, 114.0F, 272.0F, 42.0F};
inline constexpr UiRect SubmitButton{24.0F, 195.0F, 272.0F, 38.0F};
inline constexpr UiRect BackButton{8.0F, 8.0F, 70.0F, 32.0F};
inline constexpr UiRect LogoutButton{2.0F, 199.0F, 66.0F, 32.0F};
}
