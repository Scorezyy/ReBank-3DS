#include "core/RsaSignature.hpp"
#include "core/Hex.hpp"

#include <algorithm>
#include <optional>
#include <vector>

namespace {
using Limbs = std::vector<std::uint32_t>;

constexpr std::array<std::uint8_t, 19> Sha256DigestInfo{
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};
constexpr std::size_t MinimumModulusBytes = 256;
constexpr std::size_t MaximumModulusBytes = 512;

Limbs toLimbs(std::span<const std::uint8_t> bigEndian, std::size_t limbCount) {
    Limbs limbs(limbCount, 0);
    for (std::size_t i = 0; i < bigEndian.size(); ++i) {
        const std::size_t position = bigEndian.size() - 1 - i;
        limbs[i / 4] |= static_cast<std::uint32_t>(bigEndian[position]) << (8 * (i % 4));
    }
    return limbs;
}

std::vector<std::uint8_t> toBytes(const Limbs& limbs, std::size_t length) {
    std::vector<std::uint8_t> bytes(length, 0);
    for (std::size_t i = 0; i < length; ++i) {
        bytes[length - 1 - i] = static_cast<std::uint8_t>(limbs[i / 4] >> (8 * (i % 4)));
    }
    return bytes;
}

bool lessThan(const Limbs& a, const Limbs& b) {
    for (std::size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i]) {
            return a[i] < b[i];
        }
    }
    return false;
}

void subtractInPlace(Limbs& a, const Limbs& b) {
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const std::uint64_t difference = static_cast<std::uint64_t>(a[i]) - b[i] - borrow;
        a[i] = static_cast<std::uint32_t>(difference);
        borrow = (difference >> 63) & 1;
    }
}

class Montgomery {
public:
    explicit Montgomery(Limbs modulus) : n_(std::move(modulus)), size_(n_.size()) {
        std::uint32_t inverse = 1;
        for (int i = 0; i < 5; ++i) {
            inverse *= 2 - n_[0] * inverse;
        }
        n0Inverse_ = static_cast<std::uint32_t>(0 - inverse);
        rSquared_ = Limbs(size_, 0);
        rSquared_[0] = 1;
        for (std::size_t bit = 0; bit < 64 * size_; ++bit) {
            doubleModulo(rSquared_);
        }
    }

    Limbs multiply(const Limbs& a, const Limbs& b) const {
        std::vector<std::uint32_t> t(size_ + 2, 0);
        for (std::size_t i = 0; i < size_; ++i) {
            std::uint64_t carry = 0;
            for (std::size_t j = 0; j < size_; ++j) {
                const std::uint64_t sum = static_cast<std::uint64_t>(a[j]) * b[i] + t[j] + carry;
                t[j] = static_cast<std::uint32_t>(sum);
                carry = sum >> 32;
            }
            std::uint64_t sum = static_cast<std::uint64_t>(t[size_]) + carry;
            t[size_] = static_cast<std::uint32_t>(sum);
            t[size_ + 1] = static_cast<std::uint32_t>(sum >> 32);

            const std::uint32_t m = t[0] * n0Inverse_;
            carry = (static_cast<std::uint64_t>(m) * n_[0] + t[0]) >> 32;
            for (std::size_t j = 1; j < size_; ++j) {
                const std::uint64_t reduced = static_cast<std::uint64_t>(m) * n_[j] + t[j] + carry;
                t[j - 1] = static_cast<std::uint32_t>(reduced);
                carry = reduced >> 32;
            }
            sum = static_cast<std::uint64_t>(t[size_]) + carry;
            t[size_ - 1] = static_cast<std::uint32_t>(sum);
            t[size_] = t[size_ + 1] + static_cast<std::uint32_t>(sum >> 32);
        }
        Limbs result(t.begin(), t.begin() + static_cast<std::ptrdiff_t>(size_));
        if (t[size_] != 0 || !lessThan(result, n_)) {
            subtractInPlace(result, n_);
        }
        return result;
    }

    Limbs toMontgomery(const Limbs& value) const { return multiply(value, rSquared_); }

    Limbs fromMontgomery(const Limbs& value) const {
        Limbs one(size_, 0);
        one[0] = 1;
        return multiply(value, one);
    }

private:
    void doubleModulo(Limbs& value) const {
        std::uint32_t carry = 0;
        for (std::uint32_t& limb : value) {
            const std::uint32_t next = limb >> 31;
            limb = (limb << 1) | carry;
            carry = next;
        }
        if (carry != 0 || !lessThan(value, n_)) {
            subtractInPlace(value, n_);
        }
    }

    Limbs n_;
    std::size_t size_;
    std::uint32_t n0Inverse_ = 0;
    Limbs rSquared_;
};
}

namespace RsaSignature {

bool verifyPkcs1Sha256(std::string_view modulusHex, const std::array<std::uint8_t, 32>& digest,
                       std::span<const std::uint8_t> signature) {
    const auto modulus = Hex::decode(modulusHex);
    if (!modulus || modulus->size() < MinimumModulusBytes || modulus->size() > MaximumModulusBytes
        || modulus->front() == 0 || (modulus->back() & 1) == 0 || signature.size() != modulus->size()) {
        return false;
    }
    const std::size_t length = modulus->size();
    const std::size_t limbCount = (length + 3) / 4;
    const Limbs n = toLimbs(*modulus, limbCount);
    const Limbs s = toLimbs(signature, limbCount);
    if (!lessThan(s, n)) {
        return false;
    }

    const Montgomery montgomery(n);
    const Limbs base = montgomery.toMontgomery(s);
    Limbs power = base;
    for (int square = 0; square < 16; ++square) {
        power = montgomery.multiply(power, power);
    }
    power = montgomery.multiply(power, base);
    const std::vector<std::uint8_t> encoded = toBytes(montgomery.fromMontgomery(power), length);

    const std::size_t paddingEnd = length - Sha256DigestInfo.size() - digest.size() - 1;
    if (paddingEnd < 10 || encoded[0] != 0x00 || encoded[1] != 0x01 || encoded[paddingEnd] != 0x00) {
        return false;
    }
    if (!std::all_of(encoded.begin() + 2, encoded.begin() + static_cast<std::ptrdiff_t>(paddingEnd),
                     [](std::uint8_t byte) { return byte == 0xFF; })) {
        return false;
    }
    const auto infoStart = encoded.begin() + static_cast<std::ptrdiff_t>(paddingEnd + 1);
    return std::equal(Sha256DigestInfo.begin(), Sha256DigestInfo.end(), infoStart)
        && std::equal(digest.begin(), digest.end(), infoStart + static_cast<std::ptrdiff_t>(Sha256DigestInfo.size()));
}

}
