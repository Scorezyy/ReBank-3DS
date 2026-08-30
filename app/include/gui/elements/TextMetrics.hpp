#pragma once

#include <citro2d.h>

#include <string>
#include <string_view>

namespace Gui {
std::string prepareText(std::string_view value, C2D_Font font);
void parseText(C2D_Text& text, C2D_Font font, C2D_TextBuf buffer, const std::string& value);
float textHeight(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size);
float textWidth(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size);
}
