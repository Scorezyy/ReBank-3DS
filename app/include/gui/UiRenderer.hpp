#pragma once

#include "gui/Theme.hpp"

#include <citro2d.h>

#include <string>
#include <string_view>

class UiRenderer {
public:
    void setFont(C2D_Font font) { font_ = font; }
    void setActiveBuffer(C2D_TextBuf buffer) { activeBuffer_ = buffer; }

    void drawText(std::string_view value, float x, float y, float size, u32 color);
    void drawCentered(std::string_view value, float centerX, float y, float size, u32 color);
    void drawRight(std::string_view value, float rightX, float y, float size, u32 color);
    float textWidth(std::string_view value, float size);
    void drawButton(const UiRect& rect, std::string_view label, bool primary);
    void drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password);

private:
    C2D_Font font_ = nullptr;
    C2D_TextBuf activeBuffer_ = nullptr;
};
