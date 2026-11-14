#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace TitleIds {
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

constexpr std::uint64_t require(std::string_view code) {
    for (const TitleMapping& mapping : TitleMappings) {
        if (mapping.code == code) {
            return mapping.titleId;
        }
    }
    throw "unknown title code";
}

std::optional<std::uint64_t> titleIdFor(std::string_view code);
std::optional<std::string_view> codeFor(std::uint64_t titleId);
}
