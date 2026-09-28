#include "account/DeviceIdentity.hpp"
#include "core/Hmac.hpp"
#include "core/Logger.hpp"

#include <3ds.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

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

std::optional<std::uint32_t> readDeviceId() {
    if (R_FAILED(psInit())) {
        return std::nullopt;
    }
    std::uint32_t deviceId = 0;
    const Result result = PS_GetDeviceId(&deviceId);
    psExit();
    if (R_FAILED(result) || deviceId == 0) {
        return std::nullopt;
    }
    return deviceId;
}

DeviceIdentity::Key deriveKey(std::span<const std::uint8_t> secret, std::string_view label) {
    return Hmac::hmacSha256(secret, std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(label.data()), label.size()));
}

}

void DeviceIdentity::resolve() {
    const std::optional<std::uint32_t> deviceId = readDeviceId();
    if (!deviceId) {
        Logger::instance().error("Device identity unavailable (PS_GetDeviceId)");
        return;
    }
    std::vector<std::uint8_t> material(4);
    for (std::size_t i = 0; i < 4; ++i) {
        material[i] = static_cast<std::uint8_t>((*deviceId >> (8 * i)) & 0xFF);
    }

    std::array<std::uint8_t, MovableSedKeyYSize> keyY{};
    if (!readMovableSedKeyY(keyY)) {
        Logger::instance().warning("Device identity: movable.sed unavailable, local storage key uses the device ID only");
        storageKey_ = deriveKey(material, "rebank-storage-key-device-id-v1");
        return;
    }
    material.insert(material.end(), keyY.begin(), keyY.end());
    fingerprint_ = Hmac::sha256Hex(std::string(material.begin(), material.end()));
    storageKey_ = deriveKey(material, "rebank-storage-key-v1");
}
