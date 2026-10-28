#include "gui/UiRenderer.hpp"
#include "gui/elements/Shapes.hpp"

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

void UiRenderer::drawPill(const UiRect& rect, float depth, u32 color) {
    drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 10.0F, depth, color);
}

void UiRenderer::drawWarmBackground() {
    const u32 bottom = C2D_Color32(255, 228, 204, 255);
    C2D_DrawRectangle(0.0F, 0.0F, 0.01F, 320.0F, 240.0F, WarmBackground, WarmBackground, bottom, bottom);
}

void UiRenderer::drawWarmTop(float eyeOffset) {
    const float seconds = static_cast<float>(static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11);
    const float breathe = 0.5F + 0.5F * std::sin(seconds * 0.9F);
    C2D_DrawRectangle(0.0F, 0.0F, 0.01F, 400.0F, 240.0F, WarmGlowTop, WarmGlowTop, WarmGlowBottom, WarmGlowBottom);
    for (int ring = 0; ring < 4; ++ring) {
        C2D_DrawCircleSolid(200.0F + eyeOffset * 0.3F, 92.0F, 0.02F, 120.0F - ring * 24.0F,
                            C2D_Color32(255, 226, 184, static_cast<u8>(18 + breathe * 10.0F)));
    }
}

void UiRenderer::drawSpinner(float centerX, float centerY, float radius, u32 color) {
    constexpr int Segments = 12;
    const float seconds = static_cast<float>(static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11);
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
    drawPill(rect, 0.2F, track);
    const float width = rect.width * std::clamp(share, 0.0F, 1.0F);
    if (width < rect.height) {
        return;
    }
    drawPill({rect.x, rect.y, width, rect.height}, 0.3F, WarmBrand);
    drawPill({rect.x + 2.0F, rect.y + 1.5F, width - 4.0F, rect.height * 0.35F}, 0.31F, C2D_Color32(255, 190, 130, 160));
}

void UiRenderer::drawCheckMark(float centerX, float centerY, float radius) {
    C2D_DrawCircleSolid(centerX, centerY + 2.0F, 0.29F, radius, WarmBrandDark);
    C2D_DrawCircleSolid(centerX, centerY, 0.3F, radius, Surface);
    C2D_DrawCircleSolid(centerX, centerY, 0.31F, radius - 4.0F, WarmBrand);
    const float s = radius / 26.0F;
    C2D_DrawLine(centerX - 12.0F * s, centerY + 1.0F * s, Surface, centerX - 3.0F * s, centerY + 10.0F * s, Surface, 5.0F * s, 0.4F);
    C2D_DrawLine(centerX - 3.0F * s, centerY + 10.0F * s, Surface, centerX + 13.0F * s, centerY - 9.0F * s, Surface, 5.0F * s, 0.4F);
}

void UiRenderer::drawWarmButton(const UiRect& rect, std::string_view label, bool primary) {
    drawPill({rect.x, rect.y + 3.0F, rect.width, rect.height}, 0.09F,
             primary ? WarmBrandDark : C2D_Color32(234, 200, 176, 255));
    drawPill(rect, 0.1F, primary ? WarmBrand : Surface);
    drawCentered(label, rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F - 9.0F, 0.58F,
                 primary ? Surface : WarmInk);
}

void UiRenderer::drawWarmField(const UiRect& rect, std::string_view label, const std::string& value, bool password) {
    drawPill({rect.x, rect.y + 2.0F, rect.width, rect.height}, 0.09F, C2D_Color32(234, 200, 176, 255));
    drawPill(rect, 0.1F, Surface);
    drawText(label, rect.x + 12.0F, rect.y + 5.0F, 0.42F, WarmMuted);
    drawText(password ? std::string(value.size(), '*') : value, rect.x + 12.0F, rect.y + 21.0F, 0.52F, WarmInk);
}
