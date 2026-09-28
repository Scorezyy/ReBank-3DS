#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace RsaSignature {
bool verifyPkcs1Sha256(std::string_view modulusHex, const std::array<std::uint8_t, 32>& digest,
                       std::span<const std::uint8_t> signature);
}
