#include "save/storage/Cartridge.hpp"

#include "platform/Logger.hpp"
#include "save/catalog/RomGameCodes.hpp"
#include "security/Hex.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace Cartridge {
namespace {
constexpr std::uint32_t SectorSize = 0x10000;
}

std::string dsGameCode() {
    FS_CardType cardType = CARD_CTR;
    const Result typeResult = FSUSER_GetCardType(&cardType);
    if (R_FAILED(typeResult) || cardType != CARD_TWL) {
        Logger::instance().info(std::string("dsGameCode: not a TWL card (result=")
            + Hex::resultCode(static_cast<std::uint32_t>(typeResult)) + ", cardType=" + std::to_string(static_cast<int>(cardType)) + ")");
        return {};
    }
    std::array<std::uint8_t, 0x3B4> header{};
    const Result headerResult = FSUSER_GetLegacyRomHeader(MEDIATYPE_GAME_CARD, 0, header.data());
    if (R_FAILED(headerResult)) {
        Logger::instance().info("dsGameCode: header read failed");
        return {};
    }
    const std::string title(reinterpret_cast<const char*>(header.data()), 12);
    const std::string gameCode(reinterpret_cast<const char*>(header.data() + 0x0C), 4);
    const auto match = RomGameCodes::catalogCode(GamePlatform::NintendoDs, gameCode);
    Logger::instance().info("dsGameCode: title=\"" + title + "\" gameCode=\"" + gameCode
        + "\" matched=" + std::string(match.value_or("none")));
    return match ? std::string(*match) : std::string{};
}

std::uint64_t titleId() {
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

DsCardRead readDsSave(std::string_view expectedCode, bool infrared, Result& result) {
    DsCardRead read;
    if (dsGameCode() != expectedCode) {
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

bool writeDsSave(CardType cardType, const std::uint8_t* data, std::size_t size,
                 const std::uint8_t* previous, std::size_t previousSize) {
    if (R_FAILED(pxiDevInit())) {
        return false;
    }
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
