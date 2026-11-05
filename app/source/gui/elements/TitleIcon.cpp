#include "gui/elements/TitleIcon.hpp"

#include <3ds.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
constexpr std::uint32_t SmdhMagic = 0x48444D53;
constexpr std::size_t SmdhSize = 0x36C0;
constexpr std::size_t LargeIconOffset = 0x24C0;
constexpr int TextureSize = 64;
constexpr int TileSize = 8;
constexpr int PixelsPerTile = TileSize * TileSize;
constexpr std::array<std::uint32_t, 5> IconFilePath = {0, 0, 2, 0x6E6F6369, 0};
}

InstalledTitles InstalledTitles::scan() {
    InstalledTitles titles;
    if (R_FAILED(amInit())) {
        return titles;
    }
    u32 count = 0;
    if (R_SUCCEEDED(AM_GetTitleCount(MEDIATYPE_SD, &count)) && count > 0) {
        titles.sd.resize(count);
        u32 read = 0;
        titles.sd.resize(R_SUCCEEDED(AM_GetTitleList(&read, MEDIATYPE_SD, count, titles.sd.data())) ? read : 0);
    }
    bool inserted = false;
    if (R_SUCCEEDED(FSUSER_CardSlotIsInserted(&inserted)) && inserted && R_SUCCEEDED(AM_GetTitleCount(MEDIATYPE_GAME_CARD, &count)) && count > 0) {
        AM_GetTitleList(nullptr, MEDIATYPE_GAME_CARD, 1, &titles.cartridge);
    }
    amExit();
    return titles;
}

std::optional<std::pair<std::uint64_t, FS_MediaType>> InstalledTitles::find(std::span<const std::string_view> titleIds) const {
    for (const std::string_view id : titleIds) {
        const std::uint64_t titleId = std::strtoull(std::string(id).c_str(), nullptr, 16);
        if (std::find(sd.begin(), sd.end(), titleId) != sd.end()) {
            return std::make_pair(titleId, MEDIATYPE_SD);
        }
        if (cartridge == titleId) {
            return std::make_pair(titleId, MEDIATYPE_GAME_CARD);
        }
    }
    return std::nullopt;
}

std::optional<TitleIcon::Pixels> TitleIcon::read(std::span<const std::string_view> titleIds, const InstalledTitles& installed) {
    const auto title = installed.find(titleIds);
    if (!title) {
        return std::nullopt;
    }
    std::array<std::uint32_t, 4> archivePath = {static_cast<std::uint32_t>(title->first), static_cast<std::uint32_t>(title->first >> 32),
                                                 title->second, 0};
    const FS_Path archive{PATH_BINARY, sizeof(archivePath), archivePath.data()};
    const FS_Path file{PATH_BINARY, sizeof(IconFilePath), IconFilePath.data()};
    Handle handle = 0;
    if (R_FAILED(FSUSER_OpenFileDirectly(&handle, ARCHIVE_SAVEDATA_AND_CONTENT, archive, file, FS_OPEN_READ, 0))) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> smdh(SmdhSize);
    u32 read = 0;
    const Result result = FSFILE_Read(handle, &read, 0, smdh.data(), SmdhSize);
    FSFILE_Close(handle);
    std::uint32_t magic = 0;
    std::memcpy(&magic, smdh.data(), sizeof(magic));
    if (R_FAILED(result) || read != SmdhSize || magic != SmdhMagic) {
        return std::nullopt;
    }
    Pixels pixels{};
    std::memcpy(pixels.data(), smdh.data() + LargeIconOffset, sizeof(pixels));
    return pixels;
}

TitleIcon::~TitleIcon() {
    release();
}

void TitleIcon::release() {
    if (loaded_) {
        C3D_TexDelete(&texture_);
        loaded_ = false;
    }
}

bool TitleIcon::create(const Pixels& pixels) {
    release();
    if (!C3D_TexInit(&texture_, TextureSize, TextureSize, GPU_RGB565)) {
        return false;
    }
    std::memset(texture_.data, 0, texture_.size);
    auto* target = static_cast<std::uint16_t*>(texture_.data);
    constexpr int SourceTiles = Size / TileSize;
    constexpr int TargetTiles = TextureSize / TileSize;
    for (int tileY = 0; tileY < SourceTiles; ++tileY) {
        for (int tileX = 0; tileX < SourceTiles; ++tileX) {
            std::memcpy(target + (tileY * TargetTiles + tileX) * PixelsPerTile, pixels.data() + (tileY * SourceTiles + tileX) * PixelsPerTile,
                        PixelsPerTile * sizeof(std::uint16_t));
        }
    }
    C3D_TexFlush(&texture_);
    C3D_TexSetFilter(&texture_, GPU_LINEAR, GPU_LINEAR);
    const float extent = static_cast<float>(Size) / static_cast<float>(TextureSize);
    subtexture_ = {Size, Size, 0.0F, 1.0F, extent, 1.0F - extent};
    loaded_ = true;
    return true;
}

void TitleIcon::draw(float x, float y, float depth, float size) const {
    if (!loaded_) {
        return;
    }
    const C2D_Image image{const_cast<C3D_Tex*>(&texture_), &subtexture_};
    const float scale = size / static_cast<float>(Size);
    C2D_DrawImageAt(image, x, y, depth, nullptr, scale, scale);
}
