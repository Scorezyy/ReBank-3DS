#include "gui/icons/IconTexture.hpp"

#include <cstring>

namespace {
constexpr std::size_t TextureSize = 64;
constexpr std::size_t TileSize = 8;
constexpr std::size_t PixelsPerTile = TileSize * TileSize;

std::size_t tiledIndex(std::size_t x, std::size_t y) {
    const std::size_t inTile = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2)
        | ((y & 4) << 3);
    return ((y / TileSize) * (TextureSize / TileSize) + x / TileSize) * PixelsPerTile + inTile;
}

void copyLinear(const IconPixels& pixels, std::uint16_t* target) {
    for (std::size_t y = 0; y < GameIconSize; ++y) {
        for (std::size_t x = 0; x < GameIconSize; ++x) {
            target[tiledIndex(x, y)] = pixels[y * GameIconSize + x];
        }
    }
}

void copyTiled(const IconPixels& pixels, std::uint16_t* target) {
    constexpr std::size_t SourceTiles = GameIconSize / TileSize;
    constexpr std::size_t TargetTiles = TextureSize / TileSize;
    for (std::size_t tileY = 0; tileY < SourceTiles; ++tileY) {
        for (std::size_t tileX = 0; tileX < SourceTiles; ++tileX) {
            std::memcpy(target + (tileY * TargetTiles + tileX) * PixelsPerTile,
                        pixels.data() + (tileY * SourceTiles + tileX) * PixelsPerTile,
                        PixelsPerTile * sizeof(std::uint16_t));
        }
    }
}
}

IconTexture::~IconTexture() {
    release();
}

void IconTexture::release() {
    if (loaded_) {
        C3D_TexDelete(&texture_);
        loaded_ = false;
    }
}

bool IconTexture::create(const IconPixels& pixels, Layout layout) {
    release();
    if (!C3D_TexInit(&texture_, TextureSize, TextureSize, GPU_RGB565)) {
        return false;
    }
    std::memset(texture_.data, 0, texture_.size);
    auto* target = static_cast<std::uint16_t*>(texture_.data);
    if (layout == Layout::Linear) {
        copyLinear(pixels, target);
    } else {
        copyTiled(pixels, target);
    }
    C3D_TexFlush(&texture_);
    C3D_TexSetFilter(&texture_, GPU_LINEAR, GPU_LINEAR);
    constexpr auto Edge = static_cast<u16>(GameIconSize);
    constexpr float Extent = static_cast<float>(GameIconSize) / static_cast<float>(TextureSize);
    subtexture_ = {Edge, Edge, 0.0F, 1.0F, Extent, 1.0F - Extent};
    loaded_ = true;
    return true;
}

void IconTexture::draw(float x, float y, float depth, float size) const {
    if (!loaded_) {
        return;
    }
    const C2D_Image image{const_cast<C3D_Tex*>(&texture_), &subtexture_};
    const float scale = size / static_cast<float>(GameIconSize);
    C2D_DrawImageAt(image, x, y, depth, nullptr, scale, scale);
}
