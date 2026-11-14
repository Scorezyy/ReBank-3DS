#include "save/storage/SaveArchive.hpp"

#include "save/storage/SaveLimits.hpp"

#include <array>

namespace SaveArchive {
namespace {
constexpr std::array<const char*, 2> SaveFileNames{"/main", "/sav.dat"};

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
}

std::shared_ptr<std::uint8_t[]> read(
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

bool hasSave(std::uint64_t titleId, FS_MediaType mediaType) {
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

bool write(std::uint64_t titleId, FS_MediaType mediaType, const std::uint8_t* data, std::size_t size) {
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
}
