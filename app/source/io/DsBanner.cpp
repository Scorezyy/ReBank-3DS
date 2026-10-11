#include "io/DsBanner.hpp"

#include <cstring>

namespace {
constexpr std::size_t BitmapOffset = 0x20;
constexpr std::size_t PaletteOffset = 0x220;
constexpr std::size_t SourceIconSize = 32;

std::uint16_t toRgb565(std::uint16_t bgr555) {
    const std::uint16_t red = bgr555 & 0x1F;
    const std::uint16_t green = (bgr555 >> 5) & 0x1F;
    const std::uint16_t blue = (bgr555 >> 10) & 0x1F;
    return static_cast<std::uint16_t>((red << 11) | ((green << 1) << 5) | blue);
}

std::uint8_t paletteIndex(const std::uint8_t* bitmap, std::size_t x, std::size_t y) {
    const std::size_t tileOffset = ((y / 8) * 4 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2;
    const std::uint8_t packed = bitmap[tileOffset];
    return (x & 1) ? packed >> 4 : packed & 0x0F;
}
}

namespace DsBanner {
void decodeIcon(const std::uint8_t* banner, IconPixels& pixels) {
    const std::uint8_t* bitmap = banner + BitmapOffset;
    std::uint16_t palette[16];
    std::memcpy(palette, banner + PaletteOffset, sizeof(palette));
    for (std::size_t y = 0; y < GameIconSize; ++y) {
        for (std::size_t x = 0; x < GameIconSize; ++x) {
            const std::uint8_t index = paletteIndex(bitmap, x * SourceIconSize / GameIconSize,
                                                    y * SourceIconSize / GameIconSize);
            pixels[y * GameIconSize + x] = toRgb565(palette[index]);
        }
    }
}
}
