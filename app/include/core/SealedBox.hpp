#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace SealedBox {
using Key = std::array<std::uint8_t, 32>;
using Nonce = std::array<std::uint8_t, 16>;

std::vector<std::uint8_t> seal(const Key& key, const Nonce& nonce, std::span<const std::uint8_t> plaintext);
std::optional<std::vector<std::uint8_t>> open(const Key& key, std::span<const std::uint8_t> sealed);
bool looksSealed(std::span<const std::uint8_t> data);
}
