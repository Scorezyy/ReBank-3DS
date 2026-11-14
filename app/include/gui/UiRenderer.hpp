#pragma once

#include "gui/Theme.hpp"

#include <citro2d.h>

#include <string>
#include <string_view>
#include <vector>

class UiRenderer {
public:
    void setFont(C2D_Font font) { font_ = font; }
    void setActiveBuffer(C2D_TextBuf buffer) { activeBuffer_ = buffer; }

    void drawText(std::string_view value, float x, float y, float size, u32 color);
    void drawCentered(std::string_view value, float centerX, float y, float size, u32 color);
    void drawRight(std::string_view value, float rightX, float y, float size, u32 color);
    float textWidth(std::string_view value, float size);
    float measure(std::string_view value, float size) const;
    float lineHeight(float size) const;
    std::vector<std::string> wrap(std::string_view value, float size, float width) const;
    void drawWarmBackground();
    void drawWarmTop(float eyeOffset);
    void drawSpinner(float centerX, float centerY, float radius, u32 color);
    void drawProgressBar(const UiRect& rect, float share, u32 track);
    void drawBusyBar(const UiRect& rect, u32 track);
    void drawCheckMark(float centerX, float centerY, float radius);
    void drawButton(const UiRect& rect, std::string_view label, bool primary);
    void drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password);

private:
    enum class Anchor { Left, Center, Right };

    C2D_Text parse(std::string_view value);
    std::size_t fittingPrefix(std::string_view word, float size, float width) const;
    void wrapParagraph(std::string_view paragraph, float size, float width, std::vector<std::string>& lines) const;
    void draw(std::string_view value, float x, float y, float size, u32 color, Anchor anchor);

    C2D_Font font_ = nullptr;
    C2D_TextBuf activeBuffer_ = nullptr;
};
