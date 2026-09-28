#include "account/SessionStore.hpp"

#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"
#include "core/Logger.hpp"
#include "core/SealedBox.hpp"

#include <3ds.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
const std::string SessionPath = AppPaths::file("session.tok");
const std::string SessionStagingPath = SessionPath + ".tmp";
const std::string LegacyCredentialsPath = AppPaths::file("creds.bin");
constexpr std::size_t MaximumTokenSize = 512;
constexpr std::size_t MaximumUsernameSize = 64;
constexpr std::size_t MaximumFileSize = 1024;

std::vector<std::uint8_t> readAll(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return {};
    }
    std::vector<std::uint8_t> content(MaximumFileSize + 1);
    const std::size_t read = std::fread(content.data(), 1, content.size(), file);
    std::fclose(file);
    if (read == 0 || read > MaximumFileSize) {
        return {};
    }
    content.resize(read);
    return content;
}

bool writeAll(const std::vector<std::uint8_t>& content) {
    AppPaths::ensureDirectory();
    FILE* file = std::fopen(SessionStagingPath.c_str(), "wb");
    if (!file) {
        Logger::instance().warning(std::string("Session token fopen failed: ") + std::strerror(errno));
        return false;
    }
    bool ok = std::fwrite(content.data(), 1, content.size(), file) == content.size();
    ok = std::fclose(file) == 0 && ok;
    if (!ok) {
        std::remove(SessionStagingPath.c_str());
        return false;
    }
    std::remove(SessionPath.c_str());
    return std::rename(SessionStagingPath.c_str(), SessionPath.c_str()) == 0;
}

bool splitSession(const std::string& content, std::string& username, std::string& refreshToken) {
    const std::size_t separator = content.find('\n');
    if (separator == std::string::npos || separator > MaximumUsernameSize) {
        return false;
    }
    username = content.substr(0, separator);
    refreshToken = content.substr(separator + 1);
    return !refreshToken.empty() && refreshToken.size() <= MaximumTokenSize;
}
}

bool SessionStore::load(std::string& refreshToken, std::string& username) const {
    const FsGuard guard;
    const std::vector<std::uint8_t> stored = readAll(SessionPath);
    if (stored.empty()) {
        return false;
    }
    if (!SealedBox::looksSealed(stored)) {
        Logger::instance().warning("Session token stored unencrypted by an older version, it will be re-saved encrypted");
        return splitSession(std::string(stored.begin(), stored.end()), username, refreshToken);
    }
    if (!identity_.storageKey()) {
        Logger::instance().warning("Session token cannot be opened: no device storage key");
        return false;
    }
    const auto opened = SealedBox::open(*identity_.storageKey(), stored);
    if (!opened) {
        Logger::instance().error("Session token failed authentication and was ignored");
        return false;
    }
    return splitSession(std::string(opened->begin(), opened->end()), username, refreshToken);
}

bool SessionStore::save(const std::string& refreshToken, const std::string& username) const {
    if (refreshToken.empty() || refreshToken.size() > MaximumTokenSize || username.size() > MaximumUsernameSize) {
        Logger::instance().warning("SessionStore::save skipped: token missing or too large");
        return false;
    }
    if (!identity_.storageKey()) {
        Logger::instance().warning("SessionStore::save skipped: no device storage key");
        return false;
    }
    SealedBox::Nonce nonce{};
    const bool psReady = R_SUCCEEDED(psInit());
    const bool nonceReady = psReady && R_SUCCEEDED(PS_GenerateRandomBytes(nonce.data(), nonce.size()));
    if (psReady) {
        psExit();
    }
    if (!nonceReady) {
        Logger::instance().error("SessionStore::save skipped: no random nonce");
        return false;
    }
    const std::string content = username + "\n" + refreshToken;
    const auto sealed = SealedBox::seal(*identity_.storageKey(), nonce,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(content.data()), content.size()));
    const FsGuard guard;
    if (!writeAll(sealed)) {
        Logger::instance().warning("Session token could not be written");
        return false;
    }
    Logger::instance().info("Session token persisted (encrypted)");
    return true;
}

void SessionStore::clear() const {
    const FsGuard guard;
    std::remove(SessionPath.c_str());
    std::remove(SessionStagingPath.c_str());
}

void SessionStore::removeLegacyCredentials() {
    const FsGuard guard;
    if (std::remove(LegacyCredentialsPath.c_str()) == 0) {
        Logger::instance().info("Removed stored password from an older version");
    }
}
