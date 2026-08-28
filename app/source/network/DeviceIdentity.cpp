#include "network/DeviceIdentity.hpp"
#include "core/Hmac.hpp"
#include "core/Logger.hpp"

#include <3ds.h>

#include <array>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint64_t MovableSedKeyYOffset = 0x110;
constexpr std::size_t MovableSedKeyYSize = 16;

bool readMovableSedKeyY(std::array<std::uint8_t, MovableSedKeyYSize>& keyY) {
    FS_Archive archive;
    if (R_FAILED(FSUSER_OpenArchive(&archive, ARCHIVE_NAND_CTR_FS, fsMakePath(PATH_EMPTY, "")))) {
        return false;
    }
    Handle file;
    const Result openResult = FSUSER_OpenFile(
        &file, archive, fsMakePath(PATH_ASCII, "/private/movable.sed"), FS_OPEN_READ, 0
    );
    if (R_FAILED(openResult)) {
        FSUSER_CloseArchive(archive);
        return false;
    }
    std::uint32_t bytesRead = 0;
    const Result readResult = FSFILE_Read(
        file, &bytesRead, MovableSedKeyYOffset, keyY.data(), static_cast<std::uint32_t>(keyY.size())
    );
    FSFILE_Close(file);
    FSUSER_CloseArchive(archive);
    return R_SUCCEEDED(readResult) && bytesRead == keyY.size();
}

}

std::string DeviceIdentity::fingerprint() {
    if (resolved_) {
        return fingerprint_;
    }
    resolved_ = true;

    if (R_FAILED(psInit())) {
        Logger::instance().error("psInit failed for device identity");
        return fingerprint_;
    }
    std::uint32_t deviceId = 0;
    const Result deviceResult = PS_GetDeviceId(&deviceId);
    psExit();
    if (R_FAILED(deviceResult) || deviceId == 0) {
        Logger::instance().error("Device identity unavailable (PS_GetDeviceId)");
        return fingerprint_;
    }

    std::array<std::uint8_t, MovableSedKeyYSize> keyY{};
    if (!readMovableSedKeyY(keyY)) {
        Logger::instance().error("Device identity unavailable (movable.sed)");
        return fingerprint_;
    }

    std::string material(4 + MovableSedKeyYSize, '\0');
    for (int i = 0; i < 4; ++i) {
        material[i] = static_cast<char>((deviceId >> (8 * i)) & 0xFF);
    }
    std::memcpy(material.data() + 4, keyY.data(), keyY.size());

    fingerprint_ = Hmac::sha256Hex(material);
    return fingerprint_;
}
