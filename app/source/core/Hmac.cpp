#include "core/Hmac.hpp"

#include "core/Hex.hpp"

#include <utils/crypto.hpp>

#include <algorithm>

namespace {
std::span<const std::uint8_t> bytesOf(const std::string& text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}
}

namespace Hmac {

Digest sha256(std::span<const std::uint8_t> data) {
    return pksm::crypto::sha256(data);
}

Digest hmacSha256(std::span<const std::uint8_t> key, std::span<const std::uint8_t> message) {
    constexpr std::size_t BlockSize = 64;
    std::array<std::uint8_t, BlockSize> keyBlock{};
    if (key.size() > BlockSize) {
        const Digest hashedKey = sha256(key);
        std::copy(hashedKey.begin(), hashedKey.end(), keyBlock.begin());
    } else {
        std::copy(key.begin(), key.end(), keyBlock.begin());
    }
    std::array<std::uint8_t, BlockSize> innerPad{};
    std::array<std::uint8_t, BlockSize> outerPad{};
    for (std::size_t i = 0; i < BlockSize; ++i) {
        innerPad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x36);
        outerPad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x5c);
    }
    pksm::crypto::SHA256 inner;
    inner.update(innerPad);
    inner.update(message);
    const Digest innerDigest = inner.finish();
    pksm::crypto::SHA256 outer;
    outer.update(outerPad);
    outer.update(innerDigest);
    return outer.finish();
}

std::string sha256Hex(const std::string& data) {
    return Hex::encode(sha256(bytesOf(data)));
}

std::string sha256HmacHex(const std::string& key, const std::string& message) {
    return Hex::encode(hmacSha256(bytesOf(key), bytesOf(message)));
}

}
