#pragma once

#include <cstdint>
#include <string>

namespace Gui {
inline std::string kilobyteProgress(std::uint32_t received, std::uint32_t total) {
    constexpr std::uint32_t BytesPerKb = 1024;
    return std::to_string(received / BytesPerKb) + " / " + std::to_string(total / BytesPerKb) + " KB";
}
}
