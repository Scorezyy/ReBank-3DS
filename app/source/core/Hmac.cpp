#include "core/Hmac.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::array<std::uint32_t, 64> RoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

std::uint32_t rotr(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32U - bits));
}

class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        state_ = {
            0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
        };
        bitLength_ = 0;
        bufferLength_ = 0;
    }

    void update(const std::uint8_t* data, std::size_t length) {
        bitLength_ += static_cast<std::uint64_t>(length) * 8U;
        while (length > 0) {
            const std::size_t take = std::min(length, buffer_.size() - bufferLength_);
            std::memcpy(buffer_.data() + bufferLength_, data, take);
            bufferLength_ += take;
            data += take;
            length -= take;
            if (bufferLength_ == buffer_.size()) {
                processBlock(buffer_.data());
                bufferLength_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 32> finish() {
        const std::uint64_t bitLengthCopy = bitLength_;
        std::uint8_t padByte = 0x80;
        update(&padByte, 1);
        padByte = 0x00;
        while (bufferLength_ != 56) {
            update(&padByte, 1);
        }
        std::array<std::uint8_t, 8> lengthBytes{};
        for (int i = 0; i < 8; ++i) {
            lengthBytes[7 - i] = static_cast<std::uint8_t>(bitLengthCopy >> (i * 8));
        }
        update(lengthBytes.data(), lengthBytes.size());

        std::array<std::uint8_t, 32> digest{};
        for (int i = 0; i < 8; ++i) {
            digest[i * 4 + 0] = static_cast<std::uint8_t>(state_[i] >> 24);
            digest[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
            digest[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
            digest[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
        }
        return digest;
    }

private:
    void processBlock(const std::uint8_t* block) {
        std::array<std::uint32_t, 64> w{};
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4 + 0]) << 24)
                 | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16)
                 | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8)
                 | static_cast<std::uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + s1 + ch + RoundConstants[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t bufferLength_ = 0;
    std::uint64_t bitLength_ = 0;
};

std::array<std::uint8_t, 32> sha256Digest(const std::uint8_t* data, std::size_t length) {
    Sha256 hasher;
    hasher.update(data, length);
    return hasher.finish();
}

std::string toHex(const std::uint8_t* data, std::size_t length) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(Digits[data[i] >> 4]);
        result.push_back(Digits[data[i] & 0x0F]);
    }
    return result;
}

}

namespace Hmac {

std::string sha256Hex(const std::string& data) {
    const auto digest = sha256Digest(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
    return toHex(digest.data(), digest.size());
}

std::string sha256HmacHex(const std::string& key, const std::string& message) {
    constexpr std::size_t BlockSize = 64;
    std::array<std::uint8_t, BlockSize> keyBlock{};
    if (key.size() > BlockSize) {
        const auto hashedKey = sha256Digest(reinterpret_cast<const std::uint8_t*>(key.data()), key.size());
        std::memcpy(keyBlock.data(), hashedKey.data(), hashedKey.size());
    } else {
        std::memcpy(keyBlock.data(), key.data(), key.size());
    }

    std::array<std::uint8_t, BlockSize> innerPad{};
    std::array<std::uint8_t, BlockSize> outerPad{};
    for (std::size_t i = 0; i < BlockSize; ++i) {
        innerPad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x36);
        outerPad[i] = static_cast<std::uint8_t>(keyBlock[i] ^ 0x5c);
    }

    Sha256 inner;
    inner.update(innerPad.data(), innerPad.size());
    inner.update(reinterpret_cast<const std::uint8_t*>(message.data()), message.size());
    const auto innerDigest = inner.finish();

    Sha256 outer;
    outer.update(outerPad.data(), outerPad.size());
    outer.update(innerDigest.data(), innerDigest.size());
    const auto outerDigest = outer.finish();

    return toHex(outerDigest.data(), outerDigest.size());
}

}
