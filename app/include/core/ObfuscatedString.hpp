#pragma once

#include <array>
#include <cstddef>
#include <string>

template <std::size_t N>
class ObfuscatedString {
public:
    constexpr ObfuscatedString(const char (&input)[N], unsigned char key) : key_(key), data_{} {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] = static_cast<char>(input[i] ^ key);
        }
    }

    std::string decode() const {
        std::string result(N - 1, '\0');
        for (std::size_t i = 0; i < N - 1; ++i) {
            result[i] = static_cast<char>(data_[i] ^ key_);
        }
        return result;
    }

private:
    unsigned char key_;
    std::array<char, N> data_;
};

template <std::size_t N>
constexpr ObfuscatedString<N> makeObfuscatedString(const char (&input)[N], unsigned char key) {
    return ObfuscatedString<N>(input, key);
}

#define OBFUSCATED_STRING(literal) makeObfuscatedString(literal, 0x5A)
