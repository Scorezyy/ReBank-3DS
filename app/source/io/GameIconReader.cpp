#include "io/GameIconReader.hpp"
#include "core/FsGuard.hpp"
#include "io/IconCache.hpp"
#include "io/SaveMedium.hpp"

#include <3ds.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

namespace {
bool readDsCartridgeIcon(std::array<std::uint16_t, 48 * 48>& pixels) {
    const std::unique_ptr<std::uint8_t[]> banner(new (std::nothrow) std::uint8_t[0x23C0]());
    if (!banner || R_FAILED(FSUSER_GetLegacyBannerData(MEDIATYPE_GAME_CARD, 0, banner.get()))) {
        return false;
    }
    const auto* bitmap = banner.get() + 0x20;
    const auto* palette = reinterpret_cast<const std::uint16_t*>(banner.get() + 0x220);
    for (std::size_t y = 0; y < 48; ++y) {
        for (std::size_t x = 0; x < 48; ++x) {
            const std::size_t sourceX = x * 32 / 48;
            const std::size_t sourceY = y * 32 / 48;
            const std::size_t tileOffset = ((sourceY / 8) * 4 + sourceX / 8) * 32
                + (sourceY % 8) * 4 + (sourceX % 8) / 2;
            const std::uint8_t packed = bitmap[tileOffset];
            const std::uint8_t index = (sourceX & 1) ? packed >> 4 : packed & 0x0F;
            const std::uint16_t bgr555 = palette[index];
            const std::uint16_t red = bgr555 & 0x1F;
            const std::uint16_t green = (bgr555 >> 5) & 0x1F;
            const std::uint16_t blue = (bgr555 >> 10) & 0x1F;
            pixels[y * 48 + x] = static_cast<std::uint16_t>(
                (red << 11) | ((green << 1) << 5) | blue);
        }
    }
    return true;
}

bool read3dsIcon(const GameDescriptor& game, bool cartridge, std::array<std::uint16_t, 48 * 48>& pixels) {
    const auto mapping = std::find_if(SaveMedium::TitleMappings.begin(), SaveMedium::TitleMappings.end(),
        [&](const auto& item) { return item.code == game.code; });
    if (mapping == SaveMedium::TitleMappings.end()) {
        return false;
    }
    const FS_MediaType mediaType = cartridge ? MEDIATYPE_GAME_CARD : MEDIATYPE_SD;
    const std::uint32_t archivePathData[4] = {
        static_cast<std::uint32_t>(mapping->titleId),
        static_cast<std::uint32_t>(mapping->titleId >> 32),
        static_cast<std::uint32_t>(mediaType),
        0
    };
    static constexpr std::uint32_t IconFilePathData[5] = {
        0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000
    };
    Handle file = 0;
    Result result = FSUSER_OpenFileDirectly(&file, ARCHIVE_SAVEDATA_AND_CONTENT,
        FS_Path{PATH_BINARY, sizeof(archivePathData), archivePathData},
        FS_Path{PATH_BINARY, sizeof(IconFilePathData), IconFilePathData},
        FS_OPEN_READ, 0);
    if (R_FAILED(result)) {
        return false;
    }
    const std::unique_ptr<std::uint8_t[]> smdh(new (std::nothrow) std::uint8_t[0x36C0]());
    if (!smdh) {
        FSFILE_Close(file);
        return false;
    }
    std::uint32_t bytesRead = 0;
    result = FSFILE_Read(file, &bytesRead, 0, smdh.get(), 0x36C0);
    FSFILE_Close(file);
    if (R_FAILED(result) || bytesRead != 0x36C0
        || std::memcmp(smdh.get(), "SMDH", 4) != 0) {
        return false;
    }
    const auto* icon = reinterpret_cast<const std::uint16_t*>(smdh.get() + 0x24C0);
    for (std::size_t tileY = 0; tileY < 6; ++tileY) {
        for (std::size_t tileX = 0; tileX < 6; ++tileX) {
            for (std::size_t pixel = 0; pixel < 64; ++pixel) {
                const std::size_t source = (tileY * 6 + tileX) * 64 + pixel;
                const std::size_t x = tileX * 8 + ((pixel & 1) | ((pixel >> 1) & 2) | ((pixel >> 2) & 4));
                const std::size_t y = tileY * 8 + (((pixel >> 1) & 1) | ((pixel >> 2) & 2) | ((pixel >> 3) & 4));
                pixels[y * 48 + x] = icon[source];
            }
        }
    }
    return true;
}
}

namespace GameIconReader {
bool read(const GameDescriptor& game, bool cartridge,
          std::array<std::uint16_t, 48 * 48>& pixels) {
    const FsGuard guard;
    if (IconCache::load(game, cartridge, pixels)) {
        return true;
    }
    pixels.fill(0);
    const bool loaded = (game.platform == GamePlatform::NintendoDs && cartridge)
        ? readDsCartridgeIcon(pixels)
        : read3dsIcon(game, cartridge, pixels);
    if (!loaded) {
        return false;
    }
    IconCache::store(game, cartridge, pixels);
    return true;
}
}
