#include "plugin/PluginInstaller.hpp"

#include "platform/FileSystem.hpp"
#include "platform/Logger.hpp"
#include "security/SignedFile.hpp"

#include <dirent.h>

#include <cstdio>
#include <string>
#include <string_view>

namespace {
constexpr std::string_view PluginFolder = "sdmc:/luma/plugins";
constexpr std::string_view DownloadFile = "sdmc:/luma/plugins/ReBank-Plugin.download";
constexpr std::string_view BackupSuffix = "_backup";
constexpr int BackupAttempts = 99;
constexpr int DownloadAttempts = 3;

bool holdsFiles(const std::string& folder) {
    DIR* directory = opendir(folder.c_str());
    if (!directory) {
        return false;
    }
    bool found = false;
    while (const dirent* entry = readdir(directory)) {
        const std::string_view name = entry->d_name;
        if (name != "." && name != "..") {
            found = true;
            break;
        }
    }
    closedir(directory);
    return found;
}

bool backUp(const std::string& folder) {
    for (int attempt = 1; attempt <= BackupAttempts; ++attempt) {
        const std::string backup = folder + std::string(BackupSuffix) + (attempt == 1 ? "" : std::to_string(attempt));
        if (FileSystem::exists(backup)) {
            continue;
        }
        if (std::rename(folder.c_str(), backup.c_str()) != 0) {
            return false;
        }
        Logger::instance().info("Plugin install: existing plugin folder kept as " + backup);
        return true;
    }
    return false;
}

bool placeFor(std::string_view game, const std::string& fileName, const std::string& download) {
    const std::string folder = std::string(PluginFolder) + "/" + std::string(game);
    const std::string target = folder + "/" + fileName;
    const std::string pending = target + ".new";
    if (!FileSystem::exists(target) && holdsFiles(folder) && !backUp(folder)) {
        Logger::instance().error("Plugin install: the existing plugin folder could not be backed up");
        return false;
    }
    FileSystem::createDirectories(folder);
    if (!FileSystem::copy(download, pending)) {
        std::remove(pending.c_str());
        return false;
    }
    std::remove(target.c_str());
    return std::rename(pending.c_str(), target.c_str()) == 0;
}
}

PluginInstallOutcome PluginInstaller::install(ReleaseApi& api, const std::string& accessToken, const PluginInstallJob& job,
                                              PluginInstallProgress& progress) {
    progress.received.store(0);
    progress.total.store(job.file.size);
    FileSystem::createDirectories(PluginFolder);
    const std::string download(DownloadFile);
    FileDownloadResult downloaded{};
    for (int attempt = 1; attempt <= DownloadAttempts; ++attempt) {
        progress.received.store(0);
        downloaded = api.downloadPlugin(job.id, job.tag, download, job.file.size, progress.received, accessToken);
        if (downloaded.success) {
            break;
        }
        Logger::instance().warning("Plugin install " + job.id + ": attempt " + std::to_string(attempt) + " failed: " + downloaded.message);
    }
    if (!downloaded.success) {
        return PluginInstallOutcome::Unreachable;
    }
    if (SignedFile::verify(download, job.file) != SignedFile::Verdict::Valid) {
        std::remove(download.c_str());
        Logger::instance().error("Plugin install refused: " + job.id + " is not signed by ReBank");
        return PluginInstallOutcome::Rejected;
    }
    bool placed = true;
    for (const std::string& game : job.folders) {
        placed = placeFor(game, job.fileName, download) && placed;
    }
    std::remove(download.c_str());
    if (!placed) {
        Logger::instance().error("Plugin install: the SD card could not be written for " + job.id);
        return PluginInstallOutcome::StorageFailed;
    }
    Logger::instance().info("Plugin " + job.id + " " + job.tag + " installed into " + std::to_string(job.folders.size()) + " game folders");
    return PluginInstallOutcome::Installed;
}
