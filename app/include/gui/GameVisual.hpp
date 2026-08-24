#pragma once

#include <3ds.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace Gui {
struct GameVisual {
    std::string_view label;
    u32 primary;
    u32 secondary;
};

GameVisual gameVisual(std::string_view code);
std::uint8_t pokemonFormatFromCode(const std::string& code);
std::string paddedTrainerId(std::uint32_t trainerId);
}
