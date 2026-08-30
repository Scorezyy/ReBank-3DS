#include "core/Base64.hpp"

namespace Base64 {

std::string encode(const std::vector<std::uint8_t>& input) {
    static constexpr char Alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);
    for (std::size_t index = 0; index < input.size(); index += 3) {
        const std::uint32_t first = input[index];
        const std::uint32_t second = index + 1 < input.size() ? input[index + 1] : 0;
        const std::uint32_t third = index + 2 < input.size() ? input[index + 2] : 0;
        const std::uint32_t value = (first << 16) | (second << 8) | third;
        output.push_back(Alphabet[(value >> 18) & 0x3F]);
        output.push_back(Alphabet[(value >> 12) & 0x3F]);
        output.push_back(index + 1 < input.size() ? Alphabet[(value >> 6) & 0x3F] : '=');
        output.push_back(index + 2 < input.size() ? Alphabet[value & 0x3F] : '=');
    }
    return output;
}

std::vector<std::uint8_t> decode(const std::string& input) {
    static constexpr int Decode[128] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    std::vector<std::uint8_t> output;
    output.reserve((input.size() * 3) / 4);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (unsigned char c : input) {
        if (c == '=' || c >= 128) {
            break;
        }
        const int v = Decode[c];
        if (v < 0) {
            continue;
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return output;
}

}
