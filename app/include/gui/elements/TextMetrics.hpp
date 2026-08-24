#pragma once

#include <citro2d.h>

#include <string>
#include <string_view>
#include <vector>

namespace Gui {
struct MusicGlyph {
    std::size_t offset;
    bool doubleNote;
};

struct PreparedText {
    std::string value;
    std::vector<MusicGlyph> musicGlyphs;
};

// Pokemon nickname/trainer-name strings can contain the game's music-note
// glyphs, which the system font can't render; prepareText replaces each one
// with a placeholder and remembers where to draw a hand-drawn note instead.
PreparedText prepareText(std::string_view value);
void parseText(C2D_Text& text, C2D_Font font, C2D_TextBuf buffer, const std::string& value);
float textHeight(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size);
float textWidth(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size);
void drawMusicGlyphs(const PreparedText& prepared, C2D_Font font, C2D_TextBuf buffer,
                     float x, float y, float size, u32 color);
}
