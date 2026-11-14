#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace Hmac {
using Digest = std::array<std::uint8_t, 32>;

Digest sha256(std::span<const std::uint8_t> data);
Digest hmacSha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> message);

std::string sha256Hex(const std::string& data);
std::string sha256HmacHex(const std::string& key, const std::string& message);
}
