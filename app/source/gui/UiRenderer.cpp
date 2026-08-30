#include "gui/UiRenderer.hpp"
#include "gui/Theme.hpp"
#include "gui/elements/TextMetrics.hpp"

#include <cmath>

using namespace Gui;

namespace {
void primeTextMode() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0);
}
}

void UiRenderer::drawText(std::string_view value, float x, float y, float size, u32 color) {
    primeTextMode();
    C2D_Text text;
    const std::string prepared = prepareText(value, font_);
    parseText(text, font_, activeBuffer_, prepared);
    const float snappedX = std::round(x);
    const float snappedY = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, snappedX, snappedY, 0.85F, size, size, color);
}

void UiRenderer::drawCentered(std::string_view value, float centerX, float y, float size, u32 color) {
    primeTextMode();
    C2D_Text text;
    const std::string prepared = prepareText(value, font_);
    parseText(text, font_, activeBuffer_, prepared);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    const float snappedX = std::round(centerX - width * 0.5F);
    const float snappedY = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, snappedX, snappedY, 0.85F, size, size, color);
}

float UiRenderer::textWidth(std::string_view value, float size) {
    primeTextMode();
    C2D_Text text;
    const std::string prepared = prepareText(value, font_);
    parseText(text, font_, activeBuffer_, prepared);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return width;
}

void UiRenderer::drawRight(std::string_view value, float rightX, float y, float size, u32 color) {
    primeTextMode();
    C2D_Text text;
    const std::string prepared = prepareText(value, font_);
    parseText(text, font_, activeBuffer_, prepared);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    const float snappedX = std::round(rightX - width);
    const float snappedY = std::round(y);
    C2D_DrawText(&text, C2D_WithColor, snappedX, snappedY, 0.85F, size, size, color);
}

void UiRenderer::drawButton(const UiRect& rect, std::string_view label, bool primary) {
    const u32 fill = primary ? Brand : Surface;
    const u32 textColor = primary ? Surface : Ink;
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, fill);
    drawCentered(label, rect.x + rect.width * 0.5F, rect.y + 11.0F, 0.58F, textColor);
}

void UiRenderer::drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password) {
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, Surface);
    drawText(label, rect.x + 10.0F, rect.y + 5.0F, 0.42F, Muted);
    std::string displayed = value;
    if (password && !value.empty()) {
        displayed.assign(value.size(), '*');
    }
    drawText(displayed, rect.x + 10.0F, rect.y + 21.0F, 0.52F, Ink);
}
