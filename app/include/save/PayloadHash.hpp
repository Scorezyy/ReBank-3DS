#pragma once

#include <cstdint>
#include <sstream>
#include <vector>

inline std::string payloadTag(const std::vector<std::uint8_t>& data) {
    if (data.empty()) {
        return "empty";
    }
    std::uint64_t hash = 14695981039346656037ull;
    for (std::uint8_t byte : data) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << std::hex << hash << "/" << data.size();
    return out.str();
}
