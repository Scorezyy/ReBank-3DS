#include "save/SaveMedium.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace SaveMedium {

std::shared_ptr<std::uint8_t[]> readFile(const std::string& path, std::size_t& size) {
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

std::shared_ptr<std::uint8_t[]> readArchive(
    std::uint64_t titleId,
    FS_MediaType mediaType,
    std::size_t& size,
    Result& result
) {
    const std::uint32_t pathData[3] = {
        static_cast<std::uint32_t>(mediaType),
        static_cast<std::uint32_t>(titleId),
        static_cast<std::uint32_t>(titleId >> 32)
    };
    FS_Archive archive{};
    result = FSUSER_OpenArchive(
        &archive,
        ARCHIVE_USER_SAVEDATA,
        FS_Path{PATH_BINARY, sizeof(pathData), pathData}
    );
    if (R_FAILED(result)) {
        return nullptr;
    }

    Handle file = 0;
    result = FSUSER_OpenFile(
        &file,
        archive,
        fsMakePath(PATH_ASCII, "/main"),
        FS_OPEN_READ,
        0
    );
    if (R_FAILED(result)) {
        FSUSER_CloseArchive(archive);
        return nullptr;
    }

    std::uint64_t fileSize = 0;
    std::uint32_t bytesRead = 0;
    result = FSFILE_GetSize(file, &fileSize);
    if (R_SUCCEEDED(result) && fileSize > 0 && fileSize <= MaximumSaveSize) {
        size = static_cast<std::size_t>(fileSize);
        auto data = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[size]());
        result = FSFILE_Read(file, &bytesRead, 0, data.get(), static_cast<std::uint32_t>(size));
        FSFILE_Close(file);
        FSUSER_CloseArchive(archive);
        if (R_SUCCEEDED(result) && bytesRead == size) {
            return data;
        }
    }

    FSFILE_Close(file);
    FSUSER_CloseArchive(archive);
    return nullptr;
}

std::shared_ptr<std::uint8_t[]> readExport(
    std::string_view code,
    std::size_t& size,
    std::string& path
) {
    const std::array paths{
        "sdmc:/3ds/ReBank/saves/" + std::string(code) + "/main",
        "sdmc:/3ds/ReBank/saves/" + std::string(code) + ".sav"
    };
    for (const auto& candidate : paths) {
        if (auto data = readFile(candidate, size)) {
            path = candidate;
            return data;
        }
    }
    return nullptr;
}

std::string dsGameCodeFromHeader() {
    FS_CardType cardType = CARD_CTR;
    if (R_FAILED(FSUSER_GetCardType(&cardType)) || cardType != CARD_TWL) {
        return {};
    }
    std::array<std::uint8_t, 0x3B4> header{};
    if (R_FAILED(FSUSER_GetLegacyRomHeader(MEDIATYPE_GAME_CARD, 0, header.data()))) {
        return {};
    }
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
    return match == mappings.end() ? std::string{} : std::string(match->second);
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
    const std::uint32_t pathData[3] = {
        static_cast<std::uint32_t>(mediaType),
        static_cast<std::uint32_t>(titleId),
        static_cast<std::uint32_t>(titleId >> 32)
    };
    FS_Archive archive{};
    Result result = FSUSER_OpenArchive(
        &archive, ARCHIVE_USER_SAVEDATA,
        FS_Path{PATH_BINARY, sizeof(pathData), pathData}
    );
    if (R_FAILED(result)) {
        return false;
    }
    Handle file = 0;
    result = FSUSER_OpenFile(
        &file, archive, fsMakePath(PATH_ASCII, "/main"),
        FS_OPEN_WRITE, 0
    );
    if (R_FAILED(result)) {
        FSUSER_CloseArchive(archive);
        return false;
    }
    std::uint32_t bytesWritten = 0;
    result = FSFILE_Write(file, &bytesWritten, 0, data, static_cast<std::uint32_t>(size), FS_WRITE_FLUSH);
    FSFILE_Close(file);
    if (R_SUCCEEDED(result)) {
        result = FSUSER_ControlArchive(archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, nullptr, 0, nullptr, 0);
    }
    FSUSER_CloseArchive(archive);
    return R_SUCCEEDED(result) && bytesWritten == size;
}

bool writeSdFile(const std::string& path, const std::uint8_t* data, std::size_t size) {
    ::mkdir("sdmc:/3ds", 0777);
    ::mkdir("sdmc:/3ds/ReBank", 0777);
    ::mkdir("sdmc:/3ds/ReBank/saves", 0777);
    const std::string backup = path + ".bak";
    FILE* backupExists = std::fopen(backup.c_str(), "rb");
    if (!backupExists) {
        FILE* src = std::fopen(path.c_str(), "rb");
        if (src) {
            FILE* dst = std::fopen(backup.c_str(), "wb");
            if (dst) {
                std::array<std::uint8_t, 4096> buffer{};
                std::size_t read = 0;
                while ((read = std::fread(buffer.data(), 1, buffer.size(), src)) > 0) {
                    std::fwrite(buffer.data(), 1, read, dst);
                }
                std::fclose(dst);
            }
            std::fclose(src);
        }
    } else {
        std::fclose(backupExists);
    }
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        return false;
    }
    const std::size_t written = std::fwrite(data, 1, size, file);
    std::fclose(file);
    return written == size;
}

bool writeDsCard(CardType cardType, const std::uint8_t* data, std::size_t size, bool infrared,
                 const std::uint8_t* previous, std::size_t previousSize) {
    if (R_FAILED(pxiDevInit())) {
        return false;
    }
    constexpr std::uint32_t SectorSize = 0x10000;
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
        if (R_FAILED(SPIEraseSector(cardType, offset))
            || R_FAILED(SPIWriteSaveData(cardType, offset, const_cast<std::uint8_t*>(data + offset), chunk))) {
            ok = false;
        } else {
            ++writtenSectors;
        }
    }
    pxiDevExit();
    (void)infrared;
    Logger::instance().info("DS card save wrote " + std::to_string(writtenSectors) + " sectors");
    return ok;
}
}
