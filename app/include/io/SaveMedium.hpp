#pragma once

#include "spi.hpp"

#include <3ds.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace SaveMedium {
constexpr std::size_t MaximumSaveSize = 0x200000;

struct TitleMapping {
    std::string_view code;
    std::uint64_t titleId;
};

inline constexpr std::array TitleMappings{
    TitleMapping{"x", 0x0004000000055D00ULL},
    TitleMapping{"y", 0x0004000000055E00ULL},
    TitleMapping{"omega-ruby", 0x000400000011C400ULL},
    TitleMapping{"alpha-sapphire", 0x000400000011C500ULL},
    TitleMapping{"sun", 0x0004000000164800ULL},
    TitleMapping{"moon", 0x0004000000175E00ULL},
    TitleMapping{"ultra-sun", 0x00040000001B5000ULL},
    TitleMapping{"ultra-moon", 0x00040000001B5100ULL}
};

std::shared_ptr<std::uint8_t[]> readFile(const std::string& path, std::size_t& size);
std::shared_ptr<std::uint8_t[]> readArchive(
    std::uint64_t titleId,
    FS_MediaType mediaType,
    std::size_t& size,
    Result& result
);
std::shared_ptr<std::uint8_t[]> readExport(
    std::string_view code,
    std::size_t& size,
    std::string& path
);
std::string dsGameCodeFromHeader();

std::uint64_t cartridgeTitleId();

struct DsCardRead {
    std::shared_ptr<std::uint8_t[]> data;
    std::size_t size = 0;
    CardType cardType = NO_CHIP;
    std::size_t capacity = 0;
};

DsCardRead readDsCard(std::string_view expectedCode, bool infrared, Result& result);

bool writeArchive(std::uint64_t titleId, FS_MediaType mediaType, const std::uint8_t* data, std::size_t size);
bool writeSdFile(const std::string& path, const std::uint8_t* data, std::size_t size);
bool writeDsCard(
    CardType cardType,
    const std::uint8_t* data,
    std::size_t size,
    bool infrared,
    const std::uint8_t* previous,
    std::size_t previousSize
);
}
