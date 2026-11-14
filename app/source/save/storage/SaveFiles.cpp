#include "save/storage/SaveFiles.hpp"

#include "platform/AppPaths.hpp"
#include "platform/FileSystem.hpp"
#include "platform/FsGuard.hpp"
#include "platform/Logger.hpp"
#include "save/storage/SaveLimits.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>

namespace SaveFiles {
namespace {
constexpr std::string_view ExportDirectory = "saves";

std::array<std::string, 2> exportPaths(std::string_view code) {
    const std::string base = AppPaths::file(std::string(ExportDirectory) + "/" + std::string(code));
    return {base + "/main", base + ".sav"};
}

bool fileHasContent(const std::string& path, const std::uint8_t* data, std::size_t size) {
    std::size_t actualSize = 0;
    const auto actual = read(path, actualSize);
    return actual && actualSize == size && std::memcmp(actual.get(), data, size) == 0;
}

void ensureOriginalBackup(const std::string& path) {
    const std::string backup = path + ".bak";
    if (FileSystem::exists(backup) || !FileSystem::exists(path)) {
        return;
    }
    std::size_t size = 0;
    const auto original = read(path, size);
    if (!original) {
        Logger::instance().warning("Original save backup skipped: " + path + " could not be read");
        return;
    }
    const std::string staging = backup + ".tmp";
    if (!FileSystem::write(staging, std::span(original.get(), size)) || !fileHasContent(staging, original.get(), size)
        || std::rename(staging.c_str(), backup.c_str()) != 0) {
        std::remove(staging.c_str());
        Logger::instance().warning("Original save backup could not be written for " + path);
    }
}

void recoverInterruptedReplace(const std::string& path) {
    if (FileSystem::exists(path)) {
        return;
    }
    for (const char* suffix : {".tmp", ".prev"}) {
        const std::string candidate = path + suffix;
        if (FileSystem::exists(candidate) && std::rename(candidate.c_str(), path.c_str()) == 0) {
            Logger::instance().warning("Recovered " + path + " from " + candidate + " after an interrupted write");
            return;
        }
    }
}

std::array<std::string, 2> recoveredExportPaths(std::string_view code) {
    const std::array<std::string, 2> paths = exportPaths(code);
    for (const std::string& path : paths) {
        recoverInterruptedReplace(path);
    }
    return paths;
}
}

std::shared_ptr<std::uint8_t[]> read(const std::string& path, std::size_t& size) {
    const FsGuard guard;
    const std::optional<std::size_t> fileSize = FileSystem::size(path);
    if (!fileSize || *fileSize == 0 || *fileSize > MaximumSaveSize) {
        return nullptr;
    }
    auto data = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[*fileSize]());
    if (!FileSystem::readAt(path, 0, std::span(data.get(), *fileSize))) {
        return nullptr;
    }
    size = *fileSize;
    return data;
}

std::shared_ptr<std::uint8_t[]> readExport(
    std::string_view code,
    std::size_t& size,
    std::string& path
) {
    const FsGuard guard;
    for (const std::string& candidate : recoveredExportPaths(code)) {
        if (auto data = read(candidate, size)) {
            path = candidate;
            return data;
        }
    }
    return nullptr;
}

bool exportExists(std::string_view code) {
    const FsGuard guard;
    const std::array<std::string, 2> candidates = recoveredExportPaths(code);
    return std::any_of(candidates.begin(), candidates.end(), FileSystem::exists);
}

bool write(const std::string& path, const std::uint8_t* data, std::size_t size) {
    const FsGuard guard;
    AppPaths::ensureDirectory(ExportDirectory);
    ensureOriginalBackup(path);

    const std::string staging = path + ".tmp";
    const std::string previous = path + ".prev";
    if (!FileSystem::write(staging, std::span(data, size)) || !fileHasContent(staging, data, size)) {
        std::remove(staging.c_str());
        Logger::instance().error("SaveFiles::write: staging copy of " + path + " could not be written");
        return false;
    }
    std::remove(previous.c_str());
    if (FileSystem::exists(path) && std::rename(path.c_str(), previous.c_str()) != 0) {
        std::remove(staging.c_str());
        Logger::instance().error("SaveFiles::write: " + path + " could not be moved aside");
        return false;
    }
    if (std::rename(staging.c_str(), path.c_str()) != 0) {
        std::rename(previous.c_str(), path.c_str());
        Logger::instance().error("SaveFiles::write: new save could not replace " + path);
        return false;
    }
    std::remove(previous.c_str());
    return true;
}
}
