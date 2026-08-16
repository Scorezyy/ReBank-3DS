#pragma once

#include <string>
#include <string_view>

namespace BuildConfig {
inline constexpr std::string_view Version = "0.1";
inline constexpr std::string_view Channel = "BETA";
inline constexpr std::string_view Author = "Jxstn";

inline std::string label() {
    return "V " + std::string(Version) + " " + std::string(Channel)
        + " . by " + std::string(Author);
}
}