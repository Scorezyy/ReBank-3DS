#include "update/SignedFile.hpp"

#include "BuildConfig.hpp"
#include "core/Base64.hpp"
#include "core/Hex.hpp"
#include "core/RsaSignature.hpp"

#include <3ds.h>
#include <utils/crypto.hpp>

#include <array>
#include <cstdio>
#include <optional>
#include <span>
#include <vector>

namespace {
constexpr std::size_t ChunkSize = 32 * 1024;

std::optional<std::array<u8, 32>> hashFile(const std::string& path, std::uint32_t expectedSize) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return std::nullopt;
    }
    pksm::crypto::SHA256 sha256;
    std::vector<u8> buffer(ChunkSize);
    std::uint32_t total = 0;
    while (const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), file)) {
        if (total + read > expectedSize) {
            std::fclose(file);
            return std::nullopt;
        }
        sha256.update(std::span<const u8>(buffer.data(), read));
        total += static_cast<std::uint32_t>(read);
    }
    const bool complete = !std::ferror(file) && std::fclose(file) == 0 && total == expectedSize;
    if (!complete) {
        return std::nullopt;
    }
    return sha256.finish();
}
}

SignedFile::Verdict SignedFile::verify(const std::string& path, const UpdateAsset& asset) {
    const std::optional<std::array<u8, 32>> digest = hashFile(path, asset.size);
    if (!digest || Hex::encode(*digest) != asset.sha256) {
        return Verdict::Corrupt;
    }
    const std::vector<std::uint8_t> signature = Base64::decode(asset.signature);
    return RsaSignature::verifyPkcs1Sha256(BuildConfig::UpdateKeyModulus, *digest, signature) ? Verdict::Valid : Verdict::Unsigned;
}
