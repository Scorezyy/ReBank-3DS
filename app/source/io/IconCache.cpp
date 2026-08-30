#include "io/IconCache.hpp"

#include "core/Logger.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace {
constexpr std::size_t IconBytes = 48 * 48 * sizeof(std::uint16_t);

std::string cachePath(const GameDescriptor& game, bool cartridge) {
    return "sdmc:/3ds/ReBank/cache/icons/" + std::string(game.code)
        + (cartridge ? "_cart.icon" : "_sd.icon");
}

void ensureDir() {
    ::mkdir("sdmc:/3ds", 0777);
    ::mkdir("sdmc:/3ds/ReBank", 0777);
    ::mkdir("sdmc:/3ds/ReBank/cache", 0777);
    ::mkdir("sdmc:/3ds/ReBank/cache/icons", 0777);
}
}

namespace IconCache {
bool load(const GameDescriptor& game, bool cartridge, std::array<std::uint16_t, 48 * 48>& pixels) {
    FILE* file = std::fopen(cachePath(game, cartridge).c_str(), "rb");
    if (!file) {
        return false;
    }
    const std::size_t bytesRead = std::fread(pixels.data(), 1, IconBytes, file);
    std::fclose(file);
    return bytesRead == IconBytes;
}

void store(const GameDescriptor& game, bool cartridge, const std::array<std::uint16_t, 48 * 48>& pixels) {
    ensureDir();
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

void invalidate(const GameDescriptor& game, bool cartridge) {
    std::remove(cachePath(game, cartridge).c_str());
}
}
