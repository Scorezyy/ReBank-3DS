#include "gui/UiRenderer.hpp"

#include <cmath>

using namespace Gui;

namespace {
constexpr float TextDepth = 0.85F;

std::size_t validUtf8Length(std::string_view value, std::size_t index) {
    const auto lead = static_cast<unsigned char>(value[index]);
    std::size_t length = 0;
    if ((lead & 0x80) == 0x00) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
    } else {
        return 0;
    }
    if (index + length > value.size()) {
        return 0;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        if ((static_cast<unsigned char>(value[index + offset]) & 0xC0) != 0x80) {
            return 0;
        }
    }
    return length;
}

std::string sanitizeUtf8(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());
    std::size_t index = 0;
    while (index < value.size()) {
        const std::size_t length = validUtf8Length(value, index);
        if (length == 0) {
            sanitized.push_back('?');
            ++index;
            continue;
        }
        sanitized.append(value.substr(index, length));
        index += length;
    }
    return sanitized;
}
}

C2D_Text UiRenderer::parse(std::string_view value) {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0);
    C2D_Text text;
    const std::string sanitized = sanitizeUtf8(value);
    C2D_TextFontParse(&text, font_, activeBuffer_, sanitized.c_str());
    return text;
}

void UiRenderer::draw(std::string_view value, float x, float y, float size, u32 color, Anchor anchor) {
    C2D_Text text = parse(value);
    float width = 0.0F;
    if (anchor != Anchor::Left) {
        C2D_TextGetDimensions(&text, size, size, &width, nullptr);
    }
    const float shift = anchor == Anchor::Center ? width * 0.5F : (anchor == Anchor::Right ? width : 0.0F);
    C2D_DrawText(&text, C2D_WithColor, std::round(x - shift), std::round(y), TextDepth, size, size, color);
}

void UiRenderer::drawText(std::string_view value, float x, float y, float size, u32 color) {
    draw(value, x, y, size, color, Anchor::Left);
}

void UiRenderer::drawCentered(std::string_view value, float centerX, float y, float size, u32 color) {
    draw(value, centerX, y, size, color, Anchor::Center);
}

void UiRenderer::drawRight(std::string_view value, float rightX, float y, float size, u32 color) {
    draw(value, rightX, y, size, color, Anchor::Right);
}

float UiRenderer::textWidth(std::string_view value, float size) {
    C2D_Text text = parse(value);
    float width = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, nullptr);
    return width;
}

void UiRenderer::drawButton(const UiRect& rect, std::string_view label, bool primary) {
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, primary ? Brand : Surface);
    drawCentered(label, rect.x + rect.width * 0.5F, rect.y + 11.0F, 0.58F, primary ? Surface : Ink);
}

void UiRenderer::drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password) {
    C2D_DrawRectSolid(rect.x, rect.y, 0.1F, rect.width, rect.height, Surface);
    drawText(label, rect.x + 10.0F, rect.y + 5.0F, 0.42F, Muted);
    drawText(password ? std::string(value.size(), '*') : value, rect.x + 10.0F, rect.y + 21.0F, 0.52F, Ink);
}
