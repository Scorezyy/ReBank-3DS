#include "update/PluginInstaller.hpp"

#include "core/Logger.hpp"
#include "update/SignedFile.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::size_t ChunkSize = 32 * 1024;
constexpr std::string_view LumaFolder = "sdmc:/luma";
constexpr std::string_view PluginFolder = "sdmc:/luma/plugins";
constexpr std::string_view DownloadFile = "sdmc:/luma/plugins/ReBank-Plugin.download";
constexpr std::string_view BackupSuffix = "_backup";
constexpr int BackupAttempts = 99;
constexpr int DownloadAttempts = 3;

void ensureFolder(const std::string& path) {
    mkdir(path.c_str(), 0777);
}

bool copyFile(const std::string& source, const std::string& destination) {
    FILE* input = std::fopen(source.c_str(), "rb");
    if (!input) {
        return false;
    }
    FILE* output = std::fopen(destination.c_str(), "wb");
    if (!output) {
        std::fclose(input);
        return false;
    }
    std::vector<unsigned char> buffer(ChunkSize);
    bool written = true;
    while (const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), input)) {
        if (std::fwrite(buffer.data(), 1, read, output) != read) {
            written = false;
            break;
        }
    }
    const bool complete = written && !std::ferror(input);
    std::fclose(input);
    return std::fclose(output) == 0 && complete;
}

bool exists(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0;
}

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
        if (exists(backup)) {
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
    if (!exists(target) && holdsFiles(folder) && !backUp(folder)) {
        Logger::instance().error("Plugin install: the existing plugin folder could not be backed up");
        return false;
    }
    ensureFolder(folder);
    if (!copyFile(download, pending)) {
        std::remove(pending.c_str());
        return false;
    }
    std::remove(target.c_str());
    return std::rename(pending.c_str(), target.c_str()) == 0;
}
}

PluginInstallOutcome PluginInstaller::install(ApiClient& api, const std::string& accessToken, const PluginInstallJob& job,
                                              PluginInstallProgress& progress) {
    progress.received.store(0);
    progress.total.store(job.file.size);
    ensureFolder(std::string(LumaFolder));
    ensureFolder(std::string(PluginFolder));
    const std::string download(DownloadFile);
    FileDownloadResult downloaded{};
    for (int attempt = 1; attempt <= DownloadAttempts; ++attempt) {
        progress.received.store(0);
        downloaded = api.downloadCatalogPlugin(job.id, job.tag, download, job.file.size, progress.received, accessToken);
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
    for (const std::string_view game : job.folders) {
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
