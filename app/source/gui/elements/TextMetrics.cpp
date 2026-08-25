#include "gui/elements/TextMetrics.hpp"

#include <algorithm>
#include <cstdint>

namespace Gui {
namespace {
constexpr std::uint32_t MusicNoteFirst = 0x2669;
constexpr std::uint32_t MusicNoteLast = 0x266C;
constexpr std::uint32_t MusicNotePua = 0xE09A;
constexpr std::uint32_t MaleSign = 0x2642;
constexpr std::uint32_t FemaleSign = 0x2640;

bool isKnownSafe(std::uint32_t codepoint) {
    return (codepoint >= 0x00A0 && codepoint <= 0x024F)
        || (codepoint >= 0x3040 && codepoint <= 0x30FF)
        || (codepoint >= 0x4E00 && codepoint <= 0x9FFF)
        || (codepoint >= 0xFF00 && codepoint <= 0xFFEF);
}

std::uint32_t decodeUtf8(std::string_view value, std::size_t& index) {
    const auto byte = static_cast<unsigned char>(value[index]);
    std::size_t length = 1;
    std::uint32_t codepoint = byte;
    if ((byte & 0xE0) == 0xC0) {
        length = 2;
        codepoint = byte & 0x1F;
    } else if ((byte & 0xF0) == 0xE0) {
        length = 3;
        codepoint = byte & 0x0F;
    } else if ((byte & 0xF8) == 0xF0) {
        length = 4;
        codepoint = byte & 0x07;
    }
    if (index + length > value.size()) {
        ++index;
        return 0xFFFD;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        const auto continuation = static_cast<unsigned char>(value[index + offset]);
        if ((continuation & 0xC0) != 0x80) {
            ++index;
            return 0xFFFD;
        }
        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }
    index += length;
    return codepoint;
}
}

PreparedText prepareText(std::string_view value, C2D_Font font) {
    PreparedText prepared;
    prepared.value.reserve(value.size());
    const int alterIndex = font ? C2D_FontGetInfo(font)->alterCharIndex : -1;
    std::size_t index = 0;
    while (index < value.size()) {
        const std::size_t start = index;
        const std::uint32_t codepoint = decodeUtf8(value, index);
        const bool musicNote = (codepoint >= MusicNoteFirst && codepoint <= MusicNoteLast)
            || codepoint == MusicNotePua;
        if (musicNote) {
            const bool doubleNote = codepoint == 0x266B || codepoint == 0x266C;
            prepared.musicGlyphs.push_back({prepared.value.size(), doubleNote});
            prepared.value.append("  ");
            continue;
        }
        const bool renderable = codepoint < 0x80
            || (isKnownSafe(codepoint)
                && C2D_FontGlyphIndexFromCodePoint(font, codepoint) != alterIndex);
        if (renderable) {
            prepared.value.append(value.substr(start, index - start));
            continue;
        }
        if (codepoint == MaleSign) {
            prepared.value.push_back('M');
        } else if (codepoint == FemaleSign) {
            prepared.value.push_back('F');
        } else {
            prepared.value.push_back('?');
        }
    }
    return prepared;
}

void parseText(C2D_Text& text, C2D_Font font, C2D_TextBuf buffer, const std::string& value) {
    if (font) {
        C2D_TextFontParse(&text, font, buffer, value.c_str());
    } else {
        C2D_TextParse(&text, buffer, value.c_str());
    }
    C2D_TextOptimize(&text);
}

float textHeight(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size) {
    C2D_Text text;
    const std::string owned(prepareText(value, font).value);
    parseText(text, font, buffer, owned);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return height;
}

float textWidth(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size) {
    C2D_Text text;
    const std::string owned(prepareText(value, font).value);
    parseText(text, font, buffer, owned);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return width;
}

void drawMusicGlyphs(const PreparedText& prepared, C2D_Font font, C2D_TextBuf buffer,
                     float x, float y, float size, u32 color) {
    for (const MusicGlyph& glyph : prepared.musicGlyphs) {
        const float glyphX = x + textWidth(font, buffer,
            std::string_view(prepared.value).substr(0, glyph.offset), size);
        const float headY = y + 19.0F * size;
        const float topY = y + 6.0F * size;
        const float radius = std::max(1.0F, 2.6F * size);
        C2D_DrawCircleSolid(glyphX + 4.0F * size, headY, 0.51F, radius, color);
        C2D_DrawRectSolid(glyphX + 5.5F * size, topY, 0.51F,
                          std::max(1.0F, 1.5F * size), 13.0F * size, color);
        if (glyph.doubleNote) {
            C2D_DrawCircleSolid(glyphX + 12.0F * size, headY - 2.0F * size,
                                0.51F, radius, color);
            C2D_DrawRectSolid(glyphX + 13.5F * size, topY, 0.51F,
                              std::max(1.0F, 1.5F * size), 11.0F * size, color);
            C2D_DrawRectSolid(glyphX + 5.5F * size, topY, 0.51F,
                              9.5F * size, std::max(1.0F, 2.0F * size), color);
        } else {
            C2D_DrawRectSolid(glyphX + 5.5F * size, topY, 0.51F,
                              5.0F * size, std::max(1.0F, 1.5F * size), color);
            C2D_DrawRectSolid(glyphX + 9.0F * size, topY, 0.51F,
                              std::max(1.0F, 1.5F * size), 4.5F * size, color);
        }
    }
}
}
