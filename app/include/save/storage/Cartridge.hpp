#pragma once

#include "spi.hpp"

#include <3ds.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Cartridge {
std::uint64_t titleId();
std::string dsGameCode();

struct DsCardRead {
    std::shared_ptr<std::uint8_t[]> data;
    std::size_t size = 0;
    CardType cardType = NO_CHIP;
    std::size_t capacity = 0;
};

DsCardRead readDsSave(std::string_view expectedCode, bool infrared, Result& result);
bool writeDsSave(CardType cardType, const std::uint8_t* data, std::size_t size, const std::uint8_t* previous,
                 std::size_t previousSize);
}
