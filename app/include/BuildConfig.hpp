#pragma once

#include <string>
#include <string_view>

namespace BuildConfig {
inline constexpr std::string_view Version = "0.2.9";
inline constexpr std::string_view Channel = "BETA";
inline constexpr std::string_view Author = "Jxstn";
#ifdef REBANK_UPDATE_KEY_MODULUS
inline constexpr std::string_view UpdateKeyModulus = REBANK_UPDATE_KEY_MODULUS;
#else
inline constexpr std::string_view UpdateKeyModulus = "";
#endif

inline std::string label() {
    return "V " + std::string(Version) + " " + std::string(Channel) + " . by " + std::string(Author);
}
}