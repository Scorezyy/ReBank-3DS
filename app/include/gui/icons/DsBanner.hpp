#pragma once

#include "gui/icons/IconPixels.hpp"

#include <cstddef>
#include <cstdint>

namespace DsBanner {
inline constexpr std::size_t IconDataSize = 0x240;

void decodeIcon(const std::uint8_t* banner, IconPixels& pixels);
}
