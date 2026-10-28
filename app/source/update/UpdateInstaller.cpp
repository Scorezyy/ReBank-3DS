#include "update/UpdateInstaller.hpp"

#include "BuildConfig.hpp"
#include "core/AppPaths.hpp"
#include "core/Hex.hpp"
#include "core/Logger.hpp"
#include "update/SignedFile.hpp"

#include <3ds.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <sys/stat.h>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t ChunkSize = 32 * 1024;
constexpr std::string_view SdPrefix = "sdmc:/";
constexpr int CiaInstallAttempts = 2;
constexpr const char* FallbackCiaDirectory = "sdmc:/cia";
constexpr const char* FallbackCiaPath = "sdmc:/cia/ReBank.cia";

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

std::string hex(Result result) {
    return Hex::resultCode(static_cast<std::uint32_t>(result));
}

Result copyIntoCia(Handle handle, FILE* file, std::atomic<std::uint32_t>& copied) {
    std::vector<u8> buffer(ChunkSize);
    u64 offset = 0;
    while (const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), file)) {
        u32 written = 0;
        Result result = FSFILE_Write(handle, &written, offset, buffer.data(), read, 0);
        if (R_SUCCEEDED(result) && written != read) {
            result = static_cast<Result>(-1);
        }
        if (R_FAILED(result)) {
            Logger::instance().error("CIA write failed at offset " + std::to_string(offset) + ": " + hex(result));
            return result;
        }
        offset += written;
        copied = static_cast<std::uint32_t>(offset);
    }
    return std::ferror(file) ? static_cast<Result>(-1) : 0;
}

UpdateInstallResult installCiaFile(const std::string& path, std::atomic<std::uint32_t>& copied) {
    const UpdateInstallResult notStarted{false, false, "CIA update installation could not start."};
    Handle handle = 0;
    Result result = AM_StartCiaInstall(MEDIATYPE_SD, &handle);
    if (R_FAILED(result)) {
        Logger::instance().error("AM_StartCiaInstall failed: " + hex(result));
        return notStarted;
    }
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        AM_CancelCIAInstall(handle);
        return notStarted;
    }
    result = copyIntoCia(handle, file, copied);
    std::fclose(file);
    if (R_FAILED(result)) {
        AM_CancelCIAInstall(handle);
    } else if (result = AM_FinishCiaInstall(handle); R_FAILED(result)) {
        Logger::instance().error("AM_FinishCiaInstall failed: " + hex(result));
    }
    return R_SUCCEEDED(result)
        ? UpdateInstallResult{true, true, "CIA update installed."}
        : UpdateInstallResult{false, false, "CIA update installation failed."};
}

UpdateInstallResult installCia(const std::string& path, std::atomic<std::uint32_t>& copied) {
    const Result result = amInit();
    if (R_FAILED(result)) {
        Logger::instance().error("amInit failed: " + hex(result));
        return {false, false, "CIA update service is unavailable."};
    }
    UpdateInstallResult installed;
    for (int attempt = 1; attempt <= CiaInstallAttempts; ++attempt) {
        copied = 0;
        installed = installCiaFile(path, copied);
        if (installed.updated) {
            break;
        }
        Logger::instance().warning("CIA install attempt " + std::to_string(attempt) + " failed");
    }
    amExit();
    if (installed.updated) {
        std::remove(path.c_str());
        return installed;
    }
    mkdir(FallbackCiaDirectory, 0777);
    std::remove(FallbackCiaPath);
    if (std::rename(path.c_str(), FallbackCiaPath) == 0) {
        Logger::instance().info(std::string("Update CIA kept for manual install at ") + FallbackCiaPath);
        installed.keptForManualInstall = true;
    } else {
        std::remove(path.c_str());
    }
    return installed;
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

UpdateInstallResult downloadAndInstall(ApiClient& api, const std::string& executablePath, bool homebrew,
                                       UpdateProgress& progress) {
    const ClientUpdate update = api.latestClientUpdate();
    if (!update.success) {
        Logger::instance().warning("Update check skipped: " + update.message);
        return {true, false, update.message};
    }
    if (!isNewerVersion(update.version)) {
        return {true, false, "ReBank is current."};
    }
    const std::string assetName = homebrew ? "ReBank.3dsx" : "ReBank.cia";
    const UpdateAsset& asset = homebrew ? update.threeDsx : update.cia;
    const std::string temporaryPath = homebrew ? executablePath + ".update" : AppPaths::file("ReBank-update.cia");
    progress.version = update.version;
    progress.total = asset.size;
    progress.stage = UpdateStage::Downloading;
    const FileDownloadResult download = api.downloadClientUpdate(update.tag, assetName, temporaryPath, asset.size,
                                                                 progress.received);
    if (!download.success) {
        return {false, false, download.message};
    }
    progress.stage = UpdateStage::Verifying;
    const SignedFile::Verdict verdict = SignedFile::verify(temporaryPath, asset);
    if (verdict == SignedFile::Verdict::Corrupt) {
        std::remove(temporaryPath.c_str());
        Logger::instance().error("Update SHA-256 verification failed");
        return {false, false, "Update verification failed."};
    }
    if (verdict == SignedFile::Verdict::Unsigned) {
        std::remove(temporaryPath.c_str());
        Logger::instance().error(BuildConfig::UpdateKeyModulus.empty()
            ? "Update refused: this build has no update signing key"
            : "Update refused: the release signature is missing or invalid");
        return {false, false, "The update is not signed by ReBank and was not installed."};
    }
    Logger::instance().info("Installing verified " + update.tag + " " + assetName);
    progress.received = 0;
    progress.stage = UpdateStage::Installing;
    UpdateInstallResult installed = homebrew ? replaceThreeDsx(executablePath, temporaryPath)
                                             : installCia(temporaryPath, progress.received);
    if (installed.updated) {
        progress.received = progress.total.load();
        progress.stage = UpdateStage::Installed;
    }
    return installed;
}
}

UpdateInstallResult UpdateInstaller::run(ApiClient& api, const std::string& executablePath, bool homebrew,
                                         UpdateProgress& progress) {
    UpdateInstallResult result = downloadAndInstall(api, executablePath, homebrew, progress);
    if (!result.updated && progress.stage.load() != UpdateStage::Checking) {
        progress.stage = UpdateStage::Failed;
    }
    return result;
}
