#include "UpdateInstaller.hpp"

#include "BuildConfig.hpp"
#include "Logger.hpp"

#include <3ds.h>
#include <utils/crypto.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <span>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t ChunkSize = 32 * 1024;
constexpr std::string_view SdPrefix = "sdmc:/";

bool parseVersion(std::string_view text, std::array<unsigned long, 3>& parts) {
    std::size_t start = 0;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const std::size_t end = index + 1 == parts.size() ? text.size() : text.find('.', start);
        if (end == std::string_view::npos || end == start) {
            return false;
        }
        unsigned long value = 0;
        for (std::size_t position = start; position < end; ++position) {
            if (text[position] < '0' || text[position] > '9') {
                return false;
            }
            value = value * 10 + static_cast<unsigned long>(text[position] - '0');
            if (value > 65535) {
                return false;
            }
        }
        parts[index] = value;
        start = end + 1;
    }
    return start == text.size() + 1;
}

bool isNewerVersion(std::string_view available) {
    std::array<unsigned long, 3> currentParts{};
    std::array<unsigned long, 3> availableParts{};
    return parseVersion(BuildConfig::Version, currentParts)
        && parseVersion(available, availableParts)
        && availableParts > currentParts;
}

std::string hashFile(const std::string& path, std::uint32_t expectedSize) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return {};
    }
    pksm::crypto::SHA256 sha256;
    std::vector<u8> buffer(ChunkSize);
    std::uint32_t total = 0;
    while (const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), file)) {
        if (total + read > expectedSize) {
            std::fclose(file);
            return {};
        }
        sha256.update(std::span<const u8>(buffer.data(), read));
        total += static_cast<std::uint32_t>(read);
    }
    const bool complete = !std::ferror(file) && std::fclose(file) == 0 && total == expectedSize;
    if (!complete) {
        return {};
    }
    static constexpr char Hex[] = "0123456789abcdef";
    const auto digest = sha256.finish();
    std::string result;
    result.reserve(64);
    for (u8 byte : digest) {
        result.push_back(Hex[byte >> 4]);
        result.push_back(Hex[byte & 0x0F]);
    }
    return result;
}

std::string hex(Result result) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%08lX", static_cast<unsigned long>(result));
    return buffer;
}

UpdateInstallResult installCia(const std::string& path) {
    Result result = amInit();
    if (R_FAILED(result)) {
        Logger::instance().error("amInit failed: " + hex(result));
        return {false, false, "CIA update service is unavailable."};
    }
    Handle handle = 0;
    result = AM_StartCiaInstall(MEDIATYPE_SD, &handle);
    if (R_FAILED(result)) {
        Logger::instance().error("AM_StartCiaInstall failed: " + hex(result));
        amExit();
        return {false, false, "CIA update installation could not start."};
    }
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        AM_CancelCIAInstall(handle);
        amExit();
        return {false, false, "CIA update installation could not start."};
    }
    std::vector<u8> buffer(ChunkSize);
    u64 offset = 0;
    while (const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), file)) {
        u32 written = 0;
        result = FSFILE_Write(handle, &written, offset, buffer.data(), read, FS_WRITE_FLUSH);
        if (R_FAILED(result) || written != read) {
            Logger::instance().error("CIA write failed: " + hex(result));
            break;
        }
        offset += written;
    }
    if (std::ferror(file)) {
        result = static_cast<Result>(-1);
    }
    std::fclose(file);
    if (R_SUCCEEDED(result)) {
        result = AM_FinishCiaInstall(handle);
        if (R_FAILED(result)) {
            Logger::instance().error("AM_FinishCiaInstall failed: " + hex(result));
        }
    } else {
        AM_CancelCIAInstall(handle);
    }
    amExit();
    std::remove(path.c_str());
    return R_SUCCEEDED(result)
        ? UpdateInstallResult{true, true, "CIA update installed."}
        : UpdateInstallResult{false, false, "CIA update installation failed."};
}

UpdateInstallResult replaceThreeDsx(const std::string& currentPath, const std::string& updatePath) {
    if (!currentPath.starts_with(SdPrefix) || !currentPath.ends_with(".3dsx")) {
        std::remove(updatePath.c_str());
        return {false, false, "The running 3DSX path could not be verified."};
    }
    const std::string backupPath = currentPath + ".backup";
    std::remove(backupPath.c_str());
    if (std::rename(currentPath.c_str(), backupPath.c_str()) != 0) {
        std::remove(updatePath.c_str());
        return {false, false, "The current 3DSX could not be backed up."};
    }
    if (std::rename(updatePath.c_str(), currentPath.c_str()) != 0) {
        std::rename(backupPath.c_str(), currentPath.c_str());
        std::remove(updatePath.c_str());
        return {false, false, "The 3DSX update could not be activated."};
    }
    std::remove(backupPath.c_str());
    return {true, true, "3DSX update installed. Restart it from the Homebrew Launcher."};
}
}

UpdateInstallResult UpdateInstaller::run(ApiClient& api, const std::string& executablePath, bool homebrew) {
    const ClientUpdate update = api.latestClientUpdate();
    if (!update.success) {
        Logger::instance().warning("Update check skipped: " + update.message);
        return {true, false, update.message};
    }
    if (!isNewerVersion(update.version)) {
        return {true, false, "ReBank is current."};
    }
    const std::string asset = homebrew ? "ReBank.3dsx" : "ReBank.cia";
    const std::uint32_t expectedSize = homebrew ? update.threeDsxSize : update.ciaSize;
    const std::string expectedHash = homebrew ? update.threeDsxSha256 : update.ciaSha256;
    const std::string temporaryPath = homebrew
        ? executablePath + ".update"
        : "sdmc:/3ds/ReBank/ReBank-update.cia";
    const FileDownloadResult download = api.downloadClientUpdate(
        update.tag,
        asset,
        temporaryPath,
        expectedSize
    );
    if (!download.success) {
        return {false, false, download.message};
    }
    if (hashFile(temporaryPath, expectedSize) != expectedHash) {
        std::remove(temporaryPath.c_str());
        Logger::instance().error("Update SHA-256 verification failed");
        return {false, false, "Update verification failed."};
    }
    Logger::instance().info("Installing verified " + update.tag + " " + asset);
    return homebrew ? replaceThreeDsx(executablePath, temporaryPath) : installCia(temporaryPath);
}