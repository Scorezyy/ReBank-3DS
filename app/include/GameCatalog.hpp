#pragma once

#include <cstdint>
#include <span>
#include <string_view>

enum class GamePlatform {
    NintendoDs,
    Nintendo3Ds
};

enum class PokemonFormat : std::uint8_t {
    Generation4 = 4,
    Generation5 = 5,
    Generation6 = 6,
    Generation7 = 7
};

struct GameDescriptor {
    std::string_view code;
    std::string_view name;
    GamePlatform platform;
    PokemonFormat format;
};

std::span<const GameDescriptor> supportedGames();