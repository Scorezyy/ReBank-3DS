#include "core/SealedBox.hpp"

#include "core/Hmac.hpp"

#include <algorithm>
#include <string>
#include <tuple>

namespace {
constexpr std::array<std::uint8_t, 4> Magic{'R', 'B', 'S', '1'};
constexpr std::size_t TagSize = 32;
constexpr std::size_t HeaderSize = Magic.size() + std::tuple_size_v<SealedBox::Nonce>;

Hmac::Digest subkey(const SealedBox::Key& key, const char* label) {
    const std::span<const std::uint8_t> labelBytes(reinterpret_cast<const std::uint8_t*>(label),
                                                   std::char_traits<char>::length(label));
    return Hmac::hmacSha256(key, labelBytes);
}

void applyKeystream(const Hmac::Digest& key, const SealedBox::Nonce& nonce, std::span<std::uint8_t> data) {
    std::array<std::uint8_t, std::tuple_size_v<SealedBox::Nonce> + 4> block{};
    std::copy(nonce.begin(), nonce.end(), block.begin());
    for (std::size_t offset = 0; offset < data.size(); offset += 32) {
        const auto counter = static_cast<std::uint32_t>(offset / 32);
        block[nonce.size() + 0] = static_cast<std::uint8_t>(counter >> 24);
        block[nonce.size() + 1] = static_cast<std::uint8_t>(counter >> 16);
        block[nonce.size() + 2] = static_cast<std::uint8_t>(counter >> 8);
        block[nonce.size() + 3] = static_cast<std::uint8_t>(counter);
        const Hmac::Digest stream = Hmac::hmacSha256(key, block);
        const std::size_t length = std::min<std::size_t>(32, data.size() - offset);
        for (std::size_t i = 0; i < length; ++i) {
            data[offset + i] ^= stream[i];
        }
    }
}

bool constantTimeEqual(std::span<const std::uint8_t> left, std::span<const std::uint8_t> right) {
    if (left.size() != right.size()) {
        return false;
    }
    std::uint8_t difference = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        difference |= static_cast<std::uint8_t>(left[i] ^ right[i]);
    }
    return difference == 0;
}
}

namespace SealedBox {

std::vector<std::uint8_t> seal(const Key& key, const Nonce& nonce, std::span<const std::uint8_t> plaintext) {
    std::vector<std::uint8_t> sealed(Magic.begin(), Magic.end());
    sealed.insert(sealed.end(), nonce.begin(), nonce.end());
    const std::size_t bodyStart = sealed.size();
    sealed.insert(sealed.end(), plaintext.begin(), plaintext.end());
    applyKeystream(subkey(key, "rebank-sealed-enc"), nonce,
                   std::span<std::uint8_t>(sealed.data() + bodyStart, plaintext.size()));
    const Hmac::Digest tag = Hmac::hmacSha256(subkey(key, "rebank-sealed-mac"), sealed);
    sealed.insert(sealed.end(), tag.begin(), tag.end());
    return sealed;
}

std::optional<std::vector<std::uint8_t>> open(const Key& key, std::span<const std::uint8_t> sealed) {
    if (!looksSealed(sealed) || sealed.size() < HeaderSize + TagSize) {
        return std::nullopt;
    }
    const std::span<const std::uint8_t> authenticated = sealed.first(sealed.size() - TagSize);
    const Hmac::Digest expected = Hmac::hmacSha256(subkey(key, "rebank-sealed-mac"), authenticated);
    if (!constantTimeEqual(expected, sealed.last(TagSize))) {
        return std::nullopt;
    }
    Nonce nonce{};
    std::copy_n(sealed.begin() + static_cast<std::ptrdiff_t>(Magic.size()), nonce.size(), nonce.begin());
    std::vector<std::uint8_t> plaintext(authenticated.begin() + static_cast<std::ptrdiff_t>(HeaderSize),
                                        authenticated.end());
    applyKeystream(subkey(key, "rebank-sealed-enc"), nonce, plaintext);
    return plaintext;
}

bool looksSealed(std::span<const std::uint8_t> data) {
    return data.size() >= Magic.size() && std::equal(Magic.begin(), Magic.end(), data.begin());
}

}
