#include "gui/UiRenderer.hpp"

#include "gui/widgets/Shapes.hpp"
#include "platform/Clock.hpp"

#include <algorithm>
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

float UiRenderer::measure(std::string_view value, float size) const {
    float width = 0.0F;
    std::size_t index = 0;
    while (index < value.size()) {
        const auto lead = static_cast<unsigned char>(value[index]);
        const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 1;
        u32 codePoint = length == 1 ? lead : lead & (0xFF >> (length + 1));
        for (std::size_t extra = 1; extra < length && index + extra < value.size(); ++extra) {
            codePoint = (codePoint << 6) | (static_cast<unsigned char>(value[index + extra]) & 0x3F);
        }
        index += length;
        const charWidthInfo_s* info = C2D_FontGetCharWidthInfo(font_, C2D_FontGlyphIndexFromCodePoint(font_, codePoint));
        width += info ? info->charWidth * size : 0.0F;
    }
    return width;
}

std::size_t UiRenderer::fittingPrefix(std::string_view word, float size, float width) const {
    std::size_t length = word.size();
    while (length > 1 && measure(word.substr(0, length), size) > width) {
        --length;
        while (length > 1 && (static_cast<unsigned char>(word[length]) & 0xC0) == 0x80) {
            --length;
        }
    }
    return length;
}

void UiRenderer::wrapParagraph(std::string_view paragraph, float size, float width,
                               std::vector<std::string>& lines) const {
    std::string line;
    while (!paragraph.empty()) {
        const std::size_t space = paragraph.find(' ');
        std::string_view word = paragraph.substr(0, space);
        paragraph = space == std::string_view::npos ? std::string_view{} : paragraph.substr(space + 1);
        if (word.empty()) {
            continue;
        }
        const std::string candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
        if (measure(candidate, size) <= width) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
        while (measure(word, size) > width) {
            const std::size_t cut = fittingPrefix(word, size, width);
            lines.emplace_back(word.substr(0, cut));
            word = word.substr(cut);
        }
        line = std::string(word);
    }
    if (!line.empty()) {
        lines.push_back(std::move(line));
    }
}

std::vector<std::string> UiRenderer::wrap(std::string_view value, float size, float width) const {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t newline = value.find('\n', start);
        const std::size_t end = newline == std::string_view::npos ? value.size() : newline;
        wrapParagraph(value.substr(start, end - start), size, width, lines);
        if (newline == std::string_view::npos) {
            break;
        }
        start = newline + 1;
    }
    return lines;
}

float UiRenderer::lineHeight(float size) const {
    const FINF_s* info = C2D_FontGetInfo(font_);
    return (info ? info->lineFeed : 30.0F) * size;
}

float UiRenderer::textWidth(std::string_view value, float size) {
    C2D_Text text = parse(value);
    float width = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, nullptr);
    return width;
}

void UiRenderer::drawWarmBackground() {
    const u32 bottom = C2D_Color32(255, 228, 204, 255);
    C2D_DrawRectangle(0.0F, 0.0F, 0.01F, 320.0F, 240.0F, WarmBackground, WarmBackground, bottom, bottom);
}

void UiRenderer::drawWarmTop(float eyeOffset) {
    const float seconds = static_cast<float>(Clock::seconds());
    const float breathe = 0.5F + 0.5F * std::sin(seconds * 0.9F);
    C2D_DrawRectangle(0.0F, 0.0F, 0.01F, 400.0F, 240.0F, WarmGlowTop, WarmGlowTop, WarmGlowBottom, WarmGlowBottom);
    for (int ring = 0; ring < 4; ++ring) {
        C2D_DrawCircleSolid(200.0F + eyeOffset * 0.3F, 92.0F, 0.02F, 120.0F - ring * 24.0F,
                            C2D_Color32(255, 226, 184, static_cast<u8>(18 + breathe * 10.0F)));
    }
}

void UiRenderer::drawSpinner(float centerX, float centerY, float radius, u32 color) {
    constexpr int Segments = 12;
    const float seconds = static_cast<float>(Clock::seconds());
    const float head = seconds * 6.0F;
    const float dot = std::max(2.0F, radius * 0.22F);
    for (int index = 0; index < Segments; ++index) {
        const float phase = head - static_cast<float>(index) * 0.42F;
        const float fade = 1.0F - static_cast<float>(index) / Segments;
        const u32 tint = (color & 0x00FFFFFFU) | (static_cast<u32>(fade * fade * 255.0F) << 24);
        C2D_DrawCircleSolid(centerX + std::cos(phase) * radius, centerY + std::sin(phase) * radius, 0.3F,
                            dot * (0.45F + 0.55F * fade), tint);
    }
}

void UiRenderer::drawProgressBar(const UiRect& rect, float share, u32 track) {
    Gui::drawRoundedRect(rect, ControlRadius, 0.2F, track);
    const float width = rect.width * std::clamp(share, 0.0F, 1.0F);
    if (width < rect.height) {
        return;
    }
    Gui::drawRoundedRect({rect.x, rect.y, width, rect.height}, ControlRadius, 0.3F, WarmBrand);
    Gui::drawRoundedRect({rect.x + 2.0F, rect.y + 1.5F, width - 4.0F, rect.height * 0.35F}, ControlRadius, 0.31F, C2D_Color32(255, 190, 130, 160));
}

void UiRenderer::drawBusyBar(const UiRect& rect, u32 track) {
    constexpr float BlockWidth = 60.0F;
    const float cycle = std::fmod(static_cast<float>(Clock::seconds()) * 0.65F, 1.0F);
    Gui::drawRoundedRect(rect, ControlRadius, 0.2F, track);
    Gui::drawRoundedRect({rect.x + cycle * (rect.width - BlockWidth), rect.y, BlockWidth, rect.height}, ControlRadius, 0.3F,
                         WarmBrand);
}

void UiRenderer::drawCheckMark(float centerX, float centerY, float radius) {
    C2D_DrawCircleSolid(centerX, centerY + 2.0F, 0.29F, radius, WarmBrandDark);
    C2D_DrawCircleSolid(centerX, centerY, 0.3F, radius, Surface);
    C2D_DrawCircleSolid(centerX, centerY, 0.31F, radius - 4.0F, WarmBrand);
    const float s = radius / 26.0F;
    C2D_DrawLine(centerX - 12.0F * s, centerY + 1.0F * s, Surface, centerX - 3.0F * s, centerY + 10.0F * s, Surface, 5.0F * s, 0.4F);
    C2D_DrawLine(centerX - 3.0F * s, centerY + 10.0F * s, Surface, centerX + 13.0F * s, centerY - 9.0F * s, Surface, 5.0F * s, 0.4F);
}

void UiRenderer::drawButton(const UiRect& rect, std::string_view label, bool primary) {
    Gui::drawRoundedRect({rect.x, rect.y + 3.0F, rect.width, rect.height}, ControlRadius, 0.09F,
             primary ? WarmBrandDark : C2D_Color32(234, 200, 176, 255));
    Gui::drawRoundedRect(rect, ControlRadius, 0.1F, primary ? WarmBrand : Surface);
    drawCentered(label, rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F - 9.0F, 0.58F,
                 primary ? Surface : WarmInk);
}

void UiRenderer::drawField(const UiRect& rect, std::string_view label, const std::string& value, bool password) {
    Gui::drawRoundedRect({rect.x, rect.y + 2.0F, rect.width, rect.height}, ControlRadius, 0.09F, C2D_Color32(234, 200, 176, 255));
    Gui::drawRoundedRect(rect, ControlRadius, 0.1F, Surface);
    drawText(label, rect.x + 12.0F, rect.y + 5.0F, 0.42F, WarmMuted);
    drawText(password ? std::string(value.size(), '*') : value, rect.x + 12.0F, rect.y + 21.0F, 0.52F, WarmInk);
}
