#include "save/catalog/RomGameCodes.hpp"

#include <algorithm>
#include <array>

namespace {
constexpr std::size_t PrefixLength = 3;

struct GameCodeMapping {
    GamePlatform platform;
    std::string_view prefix;
    std::string_view code;
};

constexpr std::array Mappings{
    GameCodeMapping{GamePlatform::NintendoDs, "ADA", "diamond"},
    GameCodeMapping{GamePlatform::NintendoDs, "APA", "pearl"},
    GameCodeMapping{GamePlatform::NintendoDs, "CPU", "platinum"},
    GameCodeMapping{GamePlatform::NintendoDs, "IPK", "heartgold"},
    GameCodeMapping{GamePlatform::NintendoDs, "IPG", "soulsilver"},
    GameCodeMapping{GamePlatform::NintendoDs, "IRB", "black"},
    GameCodeMapping{GamePlatform::NintendoDs, "IRA", "white"},
    GameCodeMapping{GamePlatform::NintendoDs, "IRE", "black2"},
    GameCodeMapping{GamePlatform::NintendoDs, "IRD", "white2"}
};
}

namespace RomGameCodes {
std::optional<std::string_view> catalogCode(GamePlatform platform, std::string_view gameCode) {
    const std::string_view prefix = gameCode.substr(0, PrefixLength);
    const auto match = std::find_if(Mappings.begin(), Mappings.end(), [&](const GameCodeMapping& mapping) {
        return mapping.platform == platform && mapping.prefix == prefix;
    });
    return match == Mappings.end() ? std::nullopt : std::optional(match->code);
}
}
