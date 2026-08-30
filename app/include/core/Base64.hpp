#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Base64 {
std::string encode(const std::vector<std::uint8_t>& input);
std::vector<std::uint8_t> decode(const std::string& input);
}
