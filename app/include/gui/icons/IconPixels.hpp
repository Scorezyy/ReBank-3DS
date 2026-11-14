#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t GameIconSize = 48;
using IconPixels = std::array<std::uint16_t, GameIconSize * GameIconSize>;
