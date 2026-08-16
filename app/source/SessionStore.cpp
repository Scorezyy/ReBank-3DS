#include "SessionStore.hpp"

#include "Logger.hpp"

#include <3ds.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {
constexpr const char* SdSessionPath = "sdmc:/3ds/ReBank/session.tok";
constexpr std::size_t MaximumTokenSize = 512;

void ensureDir() {
    if (::mkdir("sdmc:/3ds", 0777) != 0 && errno != EEXIST) {
        Logger::instance().warning(std::string("mkdir sdmc:/3ds failed: ") + std::strerror(errno));
    }
    if (::mkdir("sdmc:/3ds/ReBank", 0777) != 0 && errno != EEXIST) {
        Logger::instance().warning(std::string("mkdir sdmc:/3ds/ReBank failed: ") + std::strerror(errno));
    }
}
}

bool SessionStore::load(std::string& refreshToken) const {
    FILE* file = std::fopen(SdSessionPath, "rb");
    if (!file) {
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0 || static_cast<std::size_t>(size) > MaximumTokenSize) {
        std::fclose(file);
        return false;
    }
    std::array<char, MaximumTokenSize + 1> buffer{};
    const std::size_t bytesRead = std::fread(buffer.data(), 1, static_cast<std::size_t>(size), file);
    std::fclose(file);
    if (bytesRead != static_cast<std::size_t>(size)) {
        return false;
    }
    refreshToken.assign(buffer.data(), bytesRead);
    Logger::instance().info("Session token loaded from " + std::string(SdSessionPath)
                            + " (" + std::to_string(bytesRead) + " bytes)");
    return true;
}

bool SessionStore::save(const std::string& refreshToken) const {
    if (refreshToken.empty()) {
        Logger::instance().warning("SessionStore::save skipped: refresh token is empty");
        return false;
    }
    if (refreshToken.size() > MaximumTokenSize) {
        Logger::instance().warning("SessionStore::save skipped: token too large ("
                                   + std::to_string(refreshToken.size()) + " bytes)");
        return false;
    }
    ensureDir();
    FILE* file = std::fopen(SdSessionPath, "wb");
    if (!file) {
        Logger::instance().warning(std::string("Session token fopen failed: ")
                                   + std::strerror(errno) + " (path=" + SdSessionPath + ")");
        return false;
    }
    const std::size_t written = std::fwrite(refreshToken.data(), 1, refreshToken.size(), file);
    std::fclose(file);
    if (written != refreshToken.size()) {
        Logger::instance().warning("Session token short write: " + std::to_string(written)
                                   + " of " + std::to_string(refreshToken.size()));
        return false;
    }
    Logger::instance().info("Session token persisted to " + std::string(SdSessionPath)
                            + " (" + std::to_string(written) + " bytes)");
    return true;
}

void SessionStore::clear() const {
    std::remove(SdSessionPath);
}