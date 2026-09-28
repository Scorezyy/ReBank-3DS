#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Hex {
inline std::string resultCode(std::uint32_t result) {
    static constexpr char Digits[] = "0123456789ABCDEF";
    std::string text = "0x";
    for (int shift = 28; shift >= 0; shift -= 4) {
        text.push_back(Digits[(result >> shift) & 0x0F]);
    }
    return text;
}

inline std::optional<std::vector<std::uint8_t>> decode(std::string_view text) {
    if (text.size() % 2 != 0) {
        return std::nullopt;
    }
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
    };
    std::vector<std::uint8_t> bytes(text.size() / 2);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const int high = nibble(text[2 * index]);
        const int low = nibble(text[2 * index + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return bytes;
}

inline std::string encode(std::span<const std::uint8_t> bytes) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        text.push_back(Digits[byte >> 4]);
        text.push_back(Digits[byte & 0x0F]);
    }
    return text;
}
}
