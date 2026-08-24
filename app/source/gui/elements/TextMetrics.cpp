#include "gui/elements/TextMetrics.hpp"

#include <algorithm>

namespace Gui {
PreparedText prepareText(std::string_view value) {
    PreparedText prepared;
    prepared.value.reserve(value.size());
    for (std::size_t index = 0; index < value.size();) {
        const bool unicodeMusicNote = index + 2 < value.size()
            && static_cast<unsigned char>(value[index]) == 0xE2
            && static_cast<unsigned char>(value[index + 1]) == 0x99
            && static_cast<unsigned char>(value[index + 2]) >= 0xA9
            && static_cast<unsigned char>(value[index + 2]) <= 0xAC;
        const bool pokemonMusicNote = index + 2 < value.size()
            && static_cast<unsigned char>(value[index]) == 0xEE
            && static_cast<unsigned char>(value[index + 1]) == 0x82
            && static_cast<unsigned char>(value[index + 2]) == 0x9A;
        const bool musicNote = unicodeMusicNote || pokemonMusicNote;
        if (!musicNote) {
            prepared.value.push_back(value[index++]);
            continue;
        }
        const bool doubleNote = unicodeMusicNote
            && static_cast<unsigned char>(value[index + 2]) >= 0xAB;
        prepared.musicGlyphs.push_back({prepared.value.size(), doubleNote});
        prepared.value.append("  ");
        index += 3;
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
    const std::string owned(value);
    parseText(text, font, buffer, owned);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return height;
}

float textWidth(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size) {
    C2D_Text text;
    const std::string owned(value);
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
