#include "io/SaveMedium.hpp"
#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"
#include "core/Hex.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace SaveMedium {
namespace {
constexpr std::string_view ExportDirectory = "saves";
constexpr std::array<const char*, 2> SaveFileNames{"/main", "/sav.dat"};

std::array<std::string, 2> exportPaths(std::string_view code) {
    const std::string base = AppPaths::file(std::string(ExportDirectory) + "/" + std::string(code));
    return {base + "/main", base + ".sav"};
}

Result openSaveArchive(std::uint64_t titleId, FS_MediaType mediaType, FS_Archive& archive) {
    const std::uint32_t pathData[3] = {
        static_cast<std::uint32_t>(mediaType),
        static_cast<std::uint32_t>(titleId),
        static_cast<std::uint32_t>(titleId >> 32)
    };
    return FSUSER_OpenArchive(&archive, ARCHIVE_USER_SAVEDATA, FS_Path{PATH_BINARY, sizeof(pathData), pathData});
}

Result openSaveFile(FS_Archive archive, u32 flags, Handle& file) {
    Result result = -1;
    for (const char* name : SaveFileNames) {
        result = FSUSER_OpenFile(&file, archive, fsMakePath(PATH_ASCII, name), flags, 0);
        if (R_SUCCEEDED(result)) {
            break;
        }
    }
    return result;
}

bool fileExists(const std::string& path) {
    struct stat info{};
    return ::stat(path.c_str(), &info) == 0;
}

bool writeWholeFile(const std::string& path, const std::uint8_t* data, std::size_t size) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        return false;
    }
    bool ok = std::fwrite(data, 1, size, file) == size;
    ok = std::fflush(file) == 0 && ok;
    ok = std::fclose(file) == 0 && ok;
    return ok;
}

bool fileHasContent(const std::string& path, const std::uint8_t* data, std::size_t size) {
    std::size_t actualSize = 0;
    const auto actual = readFile(path, actualSize);
    return actual && actualSize == size && std::memcmp(actual.get(), data, size) == 0;
}

void ensureOriginalBackup(const std::string& path) {
    const std::string backup = path + ".bak";
    if (fileExists(backup) || !fileExists(path)) {
        return;
    }
    std::size_t size = 0;
    const auto original = readFile(path, size);
    if (!original) {
        Logger::instance().warning("Original save backup skipped: " + path + " could not be read");
        return;
    }
    const std::string staging = backup + ".tmp";
    if (!writeWholeFile(staging, original.get(), size) || !fileHasContent(staging, original.get(), size)
        || std::rename(staging.c_str(), backup.c_str()) != 0) {
        std::remove(staging.c_str());
        Logger::instance().warning("Original save backup could not be written for " + path);
    }
}

void recoverInterruptedReplace(const std::string& path) {
    if (fileExists(path)) {
        return;
    }
    for (const char* suffix : {".tmp", ".prev"}) {
        const std::string candidate = path + suffix;
        if (fileExists(candidate) && std::rename(candidate.c_str(), path.c_str()) == 0) {
            Logger::instance().warning("Recovered " + path + " from " + candidate + " after an interrupted write");
            return;
        }
    }
}
}

std::shared_ptr<std::uint8_t[]> readFile(const std::string& path, std::size_t& size) {
    const FsGuard guard;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return nullptr;
    }
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::rewind(file);
    if (fileSize <= 0 || static_cast<std::size_t>(fileSize) > MaximumSaveSize) {
        std::fclose(file);
        return nullptr;
    }
    size = static_cast<std::size_t>(fileSize);
    auto data = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[size]());
    const bool success = std::fread(data.get(), 1, size, file) == size;
    std::fclose(file);
    return success ? data : nullptr;
}

std::optional<std::uint64_t> titleIdFor(std::string_view code) {
    const auto mapping = std::find_if(TitleMappings.begin(), TitleMappings.end(),
        [&](const TitleMapping& item) { return item.code == code; });
    return mapping == TitleMappings.end() ? std::nullopt : std::optional(mapping->titleId);
}

std::optional<std::string_view> codeFor(std::uint64_t titleId) {
    const auto mapping = std::find_if(TitleMappings.begin(), TitleMappings.end(),
        [&](const TitleMapping& item) { return item.titleId == titleId; });
    return mapping == TitleMappings.end() ? std::nullopt : std::optional(mapping->code);
}

std::shared_ptr<std::uint8_t[]> readArchive(
    std::uint64_t titleId,
    FS_MediaType mediaType,
    std::size_t& size,
    Result& result
) {
    FS_Archive archive{};
    result = openSaveArchive(titleId, mediaType, archive);
    if (R_FAILED(result)) {
        return nullptr;
    }
    Handle file = 0;
    result = openSaveFile(archive, FS_OPEN_READ, file);
    std::shared_ptr<std::uint8_t[]> data;
    if (R_SUCCEEDED(result)) {
        std::uint64_t fileSize = 0;
        result = FSFILE_GetSize(file, &fileSize);
        if (R_SUCCEEDED(result) && fileSize > 0 && fileSize <= MaximumSaveSize) {
            auto buffer = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[fileSize]());
            std::uint32_t bytesRead = 0;
            result = FSFILE_Read(file, &bytesRead, 0, buffer.get(), static_cast<std::uint32_t>(fileSize));
            if (R_SUCCEEDED(result) && bytesRead == fileSize) {
                size = static_cast<std::size_t>(fileSize);
                data = std::move(buffer);
            }
        }
        FSFILE_Close(file);
    }
    FSUSER_CloseArchive(archive);
    return data;
}

std::shared_ptr<std::uint8_t[]> readExport(
    std::string_view code,
    std::size_t& size,
    std::string& path
) {
    const FsGuard guard;
    for (const std::string& candidate : exportPaths(code)) {
        recoverInterruptedReplace(candidate);
        if (auto data = readFile(candidate, size)) {
            path = candidate;
            return data;
        }
    }
    return nullptr;
}

bool archiveHasSave(std::uint64_t titleId, FS_MediaType mediaType) {
    FS_Archive archive{};
    if (R_FAILED(openSaveArchive(titleId, mediaType, archive))) {
        return false;
    }
    Handle file = 0;
    const bool found = R_SUCCEEDED(openSaveFile(archive, FS_OPEN_READ, file));
    if (found) {
        FSFILE_Close(file);
    }
    FSUSER_CloseArchive(archive);
    return found;
}

bool exportExists(std::string_view code) {
    const FsGuard guard;
    for (const std::string& candidate : exportPaths(code)) {
        recoverInterruptedReplace(candidate);
        if (fileExists(candidate)) {
            return true;
        }
    }
    return false;
}

std::string dsGameCodeFromHeader() {
    FS_CardType cardType = CARD_CTR;
    const Result typeResult = FSUSER_GetCardType(&cardType);
    if (R_FAILED(typeResult) || cardType != CARD_TWL) {
        Logger::instance().info(std::string("dsGameCodeFromHeader: not a TWL card (result=")
            + Hex::resultCode(static_cast<std::uint32_t>(typeResult)) + ", cardType=" + std::to_string(static_cast<int>(cardType)) + ")");
        return {};
    }
    std::array<std::uint8_t, 0x3B4> header{};
    const Result headerResult = FSUSER_GetLegacyRomHeader(MEDIATYPE_GAME_CARD, 0, header.data());
    if (R_FAILED(headerResult)) {
        Logger::instance().info("dsGameCodeFromHeader: header read failed");
        return {};
    }
    const std::string title(reinterpret_cast<const char*>(header.data()), 12);
    const std::string prefix(reinterpret_cast<const char*>(header.data() + 0x0C), 3);
    static constexpr std::array mappings{
        std::pair{"ADA", "diamond"}, std::pair{"APA", "pearl"},
        std::pair{"CPU", "platinum"}, std::pair{"IPK", "heartgold"},
        std::pair{"IPG", "soulsilver"}, std::pair{"IRB", "black"},
        std::pair{"IRA", "white"}, std::pair{"IRE", "black2"},
        std::pair{"IRD", "white2"}
    };
    const auto match = std::find_if(mappings.begin(), mappings.end(),
        [&](const auto& mapping) { return prefix == mapping.first; });
    Logger::instance().info("dsGameCodeFromHeader: title=\"" + title + "\" prefix=\"" + prefix
        + "\" matched=" + (match == mappings.end() ? "none" : match->second));
    return match == mappings.end() ? std::string{} : std::string(match->second);
}

std::uint64_t cartridgeTitleId() {
    if (R_FAILED(amInit())) {
        return 0;
    }
    u32 count = 0;
    std::uint64_t titleId = 0;
    if (R_SUCCEEDED(AM_GetTitleCount(MEDIATYPE_GAME_CARD, &count)) && count > 0) {
        AM_GetTitleList(nullptr, MEDIATYPE_GAME_CARD, 1, &titleId);
    }
    amExit();
    return titleId;
}

DsCardRead readDsCard(std::string_view expectedCode, bool infrared, Result& result) {
    DsCardRead read;
    if (dsGameCodeFromHeader() != expectedCode) {
        result = -1;
        return read;
    }
    result = pxiDevInit();
    if (R_FAILED(result)) {
        return read;
    }
    CardType cardType = NO_CHIP;
    result = SPIGetCardType(&cardType, infrared ? 1 : 0);
    if (R_FAILED(result) || cardType == NO_CHIP) {
        pxiDevExit();
        return read;
    }
    const std::uint32_t capacity = SPIGetCapacity(cardType);
    if (capacity != 0x80000) {
        pxiDevExit();
        return read;
    }
    auto buffer = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[capacity]());
    constexpr std::uint32_t SectorSize = 0x10000;
    for (std::uint32_t offset = 0; offset < capacity; offset += SectorSize) {
        if (R_FAILED(SPIReadSaveData(cardType, offset, buffer.get() + offset, SectorSize))) {
            pxiDevExit();
            return read;
        }
    }
    pxiDevExit();
    read.data = buffer;
    read.size = capacity;
    read.cardType = cardType;
    read.capacity = capacity;
    return read;
}

bool writeArchive(std::uint64_t titleId, FS_MediaType mediaType, const std::uint8_t* data, std::size_t size) {
    FS_Archive archive{};
    if (R_FAILED(openSaveArchive(titleId, mediaType, archive))) {
        return false;
    }
    Handle file = 0;
    Result result = openSaveFile(archive, FS_OPEN_WRITE, file);
    std::uint32_t bytesWritten = 0;
    if (R_SUCCEEDED(result)) {
        result = FSFILE_Write(file, &bytesWritten, 0, data, static_cast<std::uint32_t>(size), FS_WRITE_FLUSH);
        FSFILE_Close(file);
    }
    if (R_SUCCEEDED(result)) {
        result = FSUSER_ControlArchive(archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, nullptr, 0, nullptr, 0);
    }
    FSUSER_CloseArchive(archive);
    return R_SUCCEEDED(result) && bytesWritten == size;
}

bool writeSdFile(const std::string& path, const std::uint8_t* data, std::size_t size) {
    const FsGuard guard;
    AppPaths::ensureDirectory(ExportDirectory);
    ensureOriginalBackup(path);

    const std::string staging = path + ".tmp";
    const std::string previous = path + ".prev";
    if (!writeWholeFile(staging, data, size) || !fileHasContent(staging, data, size)) {
        std::remove(staging.c_str());
        Logger::instance().error("writeSdFile: staging copy of " + path + " could not be written");
        return false;
    }
    std::remove(previous.c_str());
    if (fileExists(path) && std::rename(path.c_str(), previous.c_str()) != 0) {
        std::remove(staging.c_str());
        Logger::instance().error("writeSdFile: " + path + " could not be moved aside");
        return false;
    }
    if (std::rename(staging.c_str(), path.c_str()) != 0) {
        std::rename(previous.c_str(), path.c_str());
        Logger::instance().error("writeSdFile: new save could not replace " + path);
        return false;
    }
    std::remove(previous.c_str());
    return true;
}

bool writeDsCard(CardType cardType, const std::uint8_t* data, std::size_t size,
                 const std::uint8_t* previous, std::size_t previousSize) {
    if (R_FAILED(pxiDevInit())) {
        return false;
    }
    constexpr std::uint32_t SectorSize = 0x10000;
    constexpr int WriteAttempts = 2;
    std::vector<std::uint8_t> readback(SectorSize);
    bool ok = true;
    std::size_t writtenSectors = 0;
    for (std::uint32_t offset = 0; offset < size && ok; offset += SectorSize) {
        const std::uint32_t chunk = std::min<std::uint32_t>(SectorSize, static_cast<std::uint32_t>(size - offset));
        const bool unchanged = previous != nullptr
            && previousSize >= static_cast<std::size_t>(offset) + chunk
            && std::memcmp(previous + offset, data + offset, chunk) == 0;
        if (unchanged) {
            continue;
        }
        bool sectorOk = false;
        for (int attempt = 0; attempt < WriteAttempts && !sectorOk; ++attempt) {
            sectorOk = R_SUCCEEDED(SPIEraseSector(cardType, offset))
                && R_SUCCEEDED(SPIWriteSaveData(cardType, offset, const_cast<std::uint8_t*>(data + offset), chunk))
                && R_SUCCEEDED(SPIReadSaveData(cardType, offset, readback.data(), chunk))
                && std::memcmp(readback.data(), data + offset, chunk) == 0;
            if (!sectorOk) {
                Logger::instance().warning("DS card sector 0x" + std::to_string(offset) + " write attempt "
                                           + std::to_string(attempt + 1) + " failed verification");
            }
        }
        ok = sectorOk;
        writtenSectors += sectorOk ? 1 : 0;
    }
    pxiDevExit();
    Logger::instance().info("DS card save wrote and verified " + std::to_string(writtenSectors) + " sectors");
    return ok;
}
}
