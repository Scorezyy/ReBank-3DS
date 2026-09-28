#include "io/GameIconReader.hpp"
#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"
#include "core/Logger.hpp"
#include "io/SaveMedium.hpp"
#include "save/catalog/VirtualConsoleTitles.hpp"

#include <3ds.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

namespace {
constexpr std::size_t IconBytes = sizeof(IconPixels);
constexpr std::string_view CacheDirectory = "cache/icons";

std::string cachePath(const GameDescriptor& game, bool cartridge) {
    return AppPaths::file(std::string(CacheDirectory) + "/" + std::string(game.code)
                          + (cartridge ? "_cart.icon" : "_sd.icon"));
}

bool loadCached(const GameDescriptor& game, bool cartridge, IconPixels& pixels) {
    FILE* file = std::fopen(cachePath(game, cartridge).c_str(), "rb");
    if (!file) {
        return false;
    }
    const std::size_t bytesRead = std::fread(pixels.data(), 1, IconBytes, file);
    std::fclose(file);
    return bytesRead == IconBytes;
}

void storeCached(const GameDescriptor& game, bool cartridge, const IconPixels& pixels) {
    AppPaths::ensureDirectory(CacheDirectory);
    const std::string path = cachePath(game, cartridge);
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        Logger::instance().warning(
            std::string("Icon cache fopen failed: ") + std::strerror(errno) + " (path=" + path + ")");
        return;
    }
    const std::size_t written = std::fwrite(pixels.data(), 1, IconBytes, file);
    std::fclose(file);
    if (written != IconBytes) {
        Logger::instance().warning("Icon cache short write for " + path);
        std::remove(path.c_str());
    }
}

bool readDsCartridgeIcon(IconPixels& pixels) {
    const std::unique_ptr<std::uint8_t[]> banner(new (std::nothrow) std::uint8_t[0x23C0]());
    if (!banner || R_FAILED(FSUSER_GetLegacyBannerData(MEDIATYPE_GAME_CARD, 0, banner.get()))) {
        return false;
    }
    const auto* bitmap = banner.get() + 0x20;
    const auto* palette = reinterpret_cast<const std::uint16_t*>(banner.get() + 0x220);
    for (std::size_t y = 0; y < GameIconSize; ++y) {
        for (std::size_t x = 0; x < GameIconSize; ++x) {
            const std::size_t sourceX = x * 32 / GameIconSize;
            const std::size_t sourceY = y * 32 / GameIconSize;
            const std::size_t tileOffset = ((sourceY / 8) * 4 + sourceX / 8) * 32
                + (sourceY % 8) * 4 + (sourceX % 8) / 2;
            const std::uint8_t packed = bitmap[tileOffset];
            const std::uint8_t index = (sourceX & 1) ? packed >> 4 : packed & 0x0F;
            const std::uint16_t bgr555 = palette[index];
            const std::uint16_t red = bgr555 & 0x1F;
            const std::uint16_t green = (bgr555 >> 5) & 0x1F;
            const std::uint16_t blue = (bgr555 >> 10) & 0x1F;
            pixels[y * GameIconSize + x] = static_cast<std::uint16_t>((red << 11) | ((green << 1) << 5) | blue);
        }
    }
    return true;
}

bool read3dsIconForTitle(std::uint64_t titleId, FS_MediaType mediaType, IconPixels& pixels) {
    const std::uint32_t archivePathData[4] = {
        static_cast<std::uint32_t>(titleId),
        static_cast<std::uint32_t>(titleId >> 32),
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
    constexpr std::uint32_t SmdhSize = 0x36C0;
    const std::unique_ptr<std::uint8_t[]> smdh(new (std::nothrow) std::uint8_t[SmdhSize]());
    std::uint32_t bytesRead = 0;
    result = smdh ? FSFILE_Read(file, &bytesRead, 0, smdh.get(), SmdhSize) : -1;
    FSFILE_Close(file);
    if (R_FAILED(result) || bytesRead != SmdhSize || std::memcmp(smdh.get(), "SMDH", 4) != 0) {
        return false;
    }
    const auto* icon = reinterpret_cast<const std::uint16_t*>(smdh.get() + 0x24C0);
    for (std::size_t tileY = 0; tileY < 6; ++tileY) {
        for (std::size_t tileX = 0; tileX < 6; ++tileX) {
            for (std::size_t pixel = 0; pixel < 64; ++pixel) {
                const std::size_t source = (tileY * 6 + tileX) * 64 + pixel;
                const std::size_t x = tileX * 8 + ((pixel & 1) | ((pixel >> 1) & 2) | ((pixel >> 2) & 4));
                const std::size_t y = tileY * 8 + (((pixel >> 1) & 1) | ((pixel >> 2) & 2) | ((pixel >> 3) & 4));
                pixels[y * GameIconSize + x] = icon[source];
            }
        }
    }
    return true;
}

bool read3dsIcon(const GameDescriptor& game, bool cartridge, IconPixels& pixels) {
    const FS_MediaType mediaType = cartridge ? MEDIATYPE_GAME_CARD : MEDIATYPE_SD;
    if (const auto titleId = SaveMedium::titleIdFor(game.code)) {
        return read3dsIconForTitle(*titleId, mediaType, pixels);
    }
    const auto vcTitleId = VirtualConsoleTitles::resolveInstalledTitleId(game.code);
    return vcTitleId && read3dsIconForTitle(*vcTitleId, mediaType, pixels);
}
}

namespace GameIconReader {
bool read(const GameDescriptor& game, bool cartridge, IconPixels& pixels) {
    const FsGuard guard;
    if (loadCached(game, cartridge, pixels)) {
        return true;
    }
    pixels.fill(0);
    const bool loaded = (game.platform == GamePlatform::NintendoDs && cartridge)
        ? readDsCartridgeIcon(pixels)
        : read3dsIcon(game, cartridge, pixels);
    if (!loaded) {
        return false;
    }
    storeCached(game, cartridge, pixels);
    return true;
}

bool readUncached(const GameDescriptor& game, bool cartridge, IconPixels& pixels) {
    const FsGuard guard;
    std::remove(cachePath(game, cartridge).c_str());
    return read(game, cartridge, pixels);
}
}
