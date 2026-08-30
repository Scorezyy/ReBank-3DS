#include "gui/elements/TextMetrics.hpp"

#include <cstdint>

namespace Gui {
namespace {
std::uint32_t decodeUtf8(std::string_view value, std::size_t& index, bool& valid) {
    const auto byte = static_cast<unsigned char>(value[index]);
    std::size_t length = 1;
    std::uint32_t codepoint = byte;
    valid = true;
    if ((byte & 0x80) == 0x00) {
        length = 1;
    } else if ((byte & 0xE0) == 0xC0) {
        length = 2;
        codepoint = byte & 0x1F;
    } else if ((byte & 0xF0) == 0xE0) {
        length = 3;
        codepoint = byte & 0x0F;
    } else if ((byte & 0xF8) == 0xF0) {
        length = 4;
        codepoint = byte & 0x07;
    } else {
        ++index;
        valid = false;
        return 0xFFFD;
    }
    if (index + length > value.size()) {
        ++index;
        valid = false;
        return 0xFFFD;
    }
    for (std::size_t offset = 1; offset < length; ++offset) {
        const auto continuation = static_cast<unsigned char>(value[index + offset]);
        if ((continuation & 0xC0) != 0x80) {
            ++index;
            valid = false;
            return 0xFFFD;
        }
        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }
    index += length;
    return codepoint;
}
}

std::string prepareText(std::string_view value, C2D_Font font) {
    (void)font;
    std::string prepared;
    prepared.reserve(value.size());
    std::size_t index = 0;
    while (index < value.size()) {
        const std::size_t start = index;
        bool valid = true;
        decodeUtf8(value, index, valid);
        if (valid) {
            prepared.append(value.substr(start, index - start));
        } else {
            prepared.push_back('?');
        }
    }
    return prepared;
}

void parseText(C2D_Text& text, C2D_Font font, C2D_TextBuf buffer, const std::string& value) {
    C2D_TextFontParse(&text, font, buffer, value.c_str());
}

namespace {
C2D_TextBuf measureBuffer() {
    static C2D_TextBuf buf = C2D_TextBufNew(512);
    return buf;
}
}

float textHeight(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size) {
    (void)buffer;
    C2D_TextBuf scratch = measureBuffer();
    C2D_TextBufClear(scratch);
    C2D_Text text;
    const std::string owned = prepareText(value, font);
    parseText(text, font, scratch, owned);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return height;
}

float textWidth(C2D_Font font, C2D_TextBuf buffer, std::string_view value, float size) {
    (void)buffer;
    C2D_TextBuf scratch = measureBuffer();
    C2D_TextBufClear(scratch);
    C2D_Text text;
    const std::string owned = prepareText(value, font);
    parseText(text, font, scratch, owned);
    float width = 0.0F;
    float height = 0.0F;
    C2D_TextGetDimensions(&text, size, size, &width, &height);
    return width;
}
}
