#include "io/RomLibrary.hpp"

#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"
#include "core/KeyValueConfig.hpp"
#include "core/Logger.hpp"
#include "io/DsBanner.hpp"
#include "io/SaveMedium.hpp"
#include "save/catalog/RomGameCodes.hpp"

#include <3ds.h>
#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr std::string_view ScanRoot = "sdmc:";
constexpr std::string_view OverrideConfig = "rom_saves.cfg";
constexpr std::string_view CacheFile = "cache/rom_library.cache";
constexpr std::string_view SaveExtension = ".sav";
constexpr char CacheSeparator = '|';
constexpr int MaximumScanDepth = 6;
constexpr std::size_t HeaderSize = 0x6C;
constexpr std::size_t GameCodeLength = 4;

struct RomFormat {
    std::string_view extension;
    GamePlatform platform;
    std::size_t gameCodeOffset;
    std::size_t bannerOffsetField;
};

constexpr std::array RomFormats{
    RomFormat{".nds", GamePlatform::NintendoDs, 0x0C, 0x68}
};

constexpr std::array SkippedDirectories{
    std::string_view{"Nintendo 3DS"}, std::string_view{"3ds"}, std::string_view{"luma"},
    std::string_view{"_nds"}, std::string_view{"gm9"}, std::string_view{"saves"}
};

struct RomEntry {
    std::string_view code;
    std::string romPath;
    std::uint32_t bannerOffset = 0;
};

std::optional<std::vector<RomEntry>>& romCache() {
    static std::optional<std::vector<RomEntry>> cache;
    return cache;
}

std::string cachePath() {
    return AppPaths::file(CacheFile);
}

bool endsWithIgnoringCase(std::string_view text, std::string_view suffix) {
    if (text.size() <= suffix.size()) {
        return false;
    }
    const std::string_view tail = text.substr(text.size() - suffix.size());
    return std::equal(tail.begin(), tail.end(), suffix.begin(),
        [](char left, char right) { return std::tolower(static_cast<unsigned char>(left)) == right; });
}

const RomFormat* formatOf(std::string_view fileName) {
    const auto match = std::find_if(RomFormats.begin(), RomFormats.end(),
        [&](const RomFormat& format) { return endsWithIgnoringCase(fileName, format.extension); });
    return match == RomFormats.end() ? nullptr : &*match;
}

bool skippedDirectory(std::string_view name) {
    return name.empty() || name.front() == '.'
        || std::find(SkippedDirectories.begin(), SkippedDirectories.end(), name) != SkippedDirectories.end();
}

bool isDirectory(const dirent& entry, const std::string& path) {
    if (entry.d_type != DT_UNKNOWN) {
        return entry.d_type == DT_DIR;
    }
    struct stat info{};
    return ::stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::optional<RomEntry> readRom(const std::string& path, const RomFormat& format) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return std::nullopt;
    }
    std::array<std::uint8_t, HeaderSize> header{};
    const bool complete = std::fread(header.data(), 1, header.size(), file) == header.size();
    std::fclose(file);
    if (!complete) {
        return std::nullopt;
    }
    const std::string_view gameCode(reinterpret_cast<const char*>(header.data() + format.gameCodeOffset),
                                    GameCodeLength);
    const auto code = RomGameCodes::catalogCode(format.platform, gameCode);
    if (!code) {
        return std::nullopt;
    }
    RomEntry entry{*code, path};
    std::memcpy(&entry.bannerOffset, header.data() + format.bannerOffsetField, sizeof(entry.bannerOffset));
    return entry;
}

std::vector<std::string> saveCandidates(const std::string& romPath) {
    const std::size_t slash = romPath.rfind('/');
    const std::string directory = romPath.substr(0, slash);
    const std::string fileName = romPath.substr(slash + 1);
    const std::string stem = fileName.substr(0, fileName.rfind('.'));
    std::vector<std::string> candidates;
    for (const std::string& name : {stem, fileName}) {
        candidates.push_back(directory + "/saves/" + name + std::string(SaveExtension));
        candidates.push_back(directory + "/" + name + std::string(SaveExtension));
    }
    return candidates;
}

std::optional<std::string> savePathFor(const std::string& romPath) {
    const std::vector<std::string> candidates = saveCandidates(romPath);
    const auto match = std::find_if(candidates.begin(), candidates.end(), SaveMedium::fileExists);
    return match == candidates.end() ? std::nullopt : std::optional(*match);
}

void scanDirectory(const std::string& path, int depth, std::vector<RomEntry>& entries) {
    DIR* directory = ::opendir(depth == 0 ? (path + "/").c_str() : path.c_str());
    if (!directory) {
        return;
    }
    while (const dirent* entry = ::readdir(directory)) {
        const std::string_view name(entry->d_name);
        const std::string childPath = path + "/" + std::string(name);
        if (const RomFormat* format = formatOf(name)) {
            if (auto rom = readRom(childPath, *format)) {
                Logger::instance().info("ROM " + childPath + " detected as " + std::string(rom->code));
                entries.push_back(std::move(*rom));
            }
        } else if (depth < MaximumScanDepth && !skippedDirectory(name) && isDirectory(*entry, childPath)) {
            scanDirectory(childPath, depth + 1, entries);
        }
    }
    ::closedir(directory);
}

std::optional<RomEntry> parseCacheLine(std::string_view line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    const std::size_t first = line.find(CacheSeparator);
    const std::size_t second = first == std::string_view::npos ? first : line.find(CacheSeparator, first + 1);
    if (second == std::string_view::npos) {
        return std::nullopt;
    }
    const GameDescriptor* game = findGame(line.substr(0, first));
    if (!game || !RomLibrary::supports(game->platform)) {
        return std::nullopt;
    }
    const std::string offset(line.substr(second + 1));
    return RomEntry{game->code, std::string(line.substr(first + 1, second - first - 1)),
                    static_cast<std::uint32_t>(std::strtoul(offset.c_str(), nullptr, 10))};
}

std::optional<std::vector<RomEntry>> loadCachedRoms() {
    FILE* file = std::fopen(cachePath().c_str(), "r");
    if (!file) {
        return std::nullopt;
    }
    std::vector<RomEntry> entries;
    bool valid = true;
    char line[512];
    while (valid && std::fgets(line, sizeof(line), file)) {
        auto entry = parseCacheLine(line);
        valid = entry && SaveMedium::fileExists(entry->romPath);
        if (valid) {
            entries.push_back(std::move(*entry));
        }
    }
    std::fclose(file);
    if (!valid) {
        Logger::instance().info("ROM library cache is outdated, rescanning");
        return std::nullopt;
    }
    Logger::instance().info("ROM library cache loaded with " + std::to_string(entries.size()) + " Pokemon ROMs");
    return entries;
}

void storeCachedRoms(const std::vector<RomEntry>& entries) {
    AppPaths::ensureDirectory("cache");
    FILE* file = std::fopen(cachePath().c_str(), "w");
    if (!file) {
        Logger::instance().warning("ROM library cache could not be written");
        return;
    }
    for (const RomEntry& entry : entries) {
        std::fprintf(file, "%.*s%c%s%c%lu\n", static_cast<int>(entry.code.size()), entry.code.data(),
                     CacheSeparator, entry.romPath.c_str(), CacheSeparator,
                     static_cast<unsigned long>(entry.bannerOffset));
    }
    std::fclose(file);
}

std::vector<RomEntry> scanLibrary() {
    const u64 start = svcGetSystemTick();
    std::vector<RomEntry> entries;
    scanDirectory(std::string(ScanRoot), 0, entries);
    storeCachedRoms(entries);
    Logger::instance().info("ROM library scan found " + std::to_string(entries.size()) + " Pokemon ROMs in "
        + std::to_string((svcGetSystemTick() - start) * 1000 / SYSCLOCK_ARM11) + "ms");
    return entries;
}

const std::vector<RomEntry>& libraryRoms() {
    if (!romCache()) {
        RomLibrary::load(false);
    }
    return *romCache();
}

std::optional<RomSave> configuredSave(std::string_view code) {
    const auto path = KeyValueConfig::lookup(OverrideConfig, code);
    if (!path || !SaveMedium::fileExists(*path)) {
        return std::nullopt;
    }
    return RomSave{{}, *path, 0};
}

std::optional<RomSave> romSave(const RomEntry& entry) {
    const auto savePath = savePathFor(entry.romPath);
    if (!savePath) {
        return std::nullopt;
    }
    return RomSave{entry.romPath, *savePath, entry.bannerOffset};
}
}

namespace RomLibrary {
bool supports(GamePlatform platform) {
    return std::any_of(RomFormats.begin(), RomFormats.end(),
        [&](const RomFormat& format) { return format.platform == platform; });
}

bool libraryCached() {
    const FsGuard guard;
    return SaveMedium::fileExists(cachePath());
}

void load(bool rebuild) {
    const FsGuard guard;
    std::optional<std::vector<RomEntry>> cached = rebuild ? std::nullopt : loadCachedRoms();
    romCache() = cached ? std::move(cached) : scanLibrary();
}

std::optional<RomSave> find(std::string_view code) {
    if (auto configured = configuredSave(code)) {
        return configured;
    }
    const FsGuard guard;
    for (const RomEntry& entry : libraryRoms()) {
        if (entry.code != code) {
            continue;
        }
        if (auto save = romSave(entry)) {
            return save;
        }
        Logger::instance().info("ROM " + entry.romPath + " has no save next to it");
    }
    return std::nullopt;
}

bool readIcon(const RomSave& rom, IconPixels& pixels) {
    if (rom.romPath.empty() || rom.bannerOffset == 0) {
        return false;
    }
    const FsGuard guard;
    FILE* file = std::fopen(rom.romPath.c_str(), "rb");
    if (!file) {
        return false;
    }
    std::array<std::uint8_t, DsBanner::IconDataSize> banner{};
    const bool complete = std::fseek(file, static_cast<long>(rom.bannerOffset), SEEK_SET) == 0
        && std::fread(banner.data(), 1, banner.size(), file) == banner.size();
    std::fclose(file);
    if (complete) {
        DsBanner::decodeIcon(banner.data(), pixels);
    }
    return complete;
}
}
