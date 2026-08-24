#include "network/CredentialStore.hpp"
#include "core/Logger.hpp"

#include <3ds.h>
#include <utils/crypto.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

namespace {
constexpr std::array<char, 4> Magic{'R', 'B', 'C', '1'};
constexpr std::size_t NonceSize = 16;
constexpr std::size_t TagSize = 16;

std::array<std::uint8_t, 32> deriveKey(std::uint32_t deviceId) {
    std::array<std::uint8_t, 20> material{};
    static constexpr char label[] = "rebank-cred-v1";
    std::memcpy(material.data(), label, sizeof(label) - 1);
    material[14] = static_cast<std::uint8_t>(deviceId & 0xFF);
    material[15] = static_cast<std::uint8_t>((deviceId >> 8) & 0xFF);
    material[16] = static_cast<std::uint8_t>((deviceId >> 16) & 0xFF);
    material[17] = static_cast<std::uint8_t>((deviceId >> 24) & 0xFF);
    material[18] = 0xA5;
    material[19] = 0x5A;
    return pksm::crypto::sha256({material.data(), material.size()});
}

void keystream(
    const std::array<std::uint8_t, 32>& key,
    const std::uint8_t* nonce,
    std::vector<std::uint8_t>& out
) {
    std::array<std::uint8_t, 32 + NonceSize + 4> block{};
    std::memcpy(block.data(), key.data(), 32);
    std::memcpy(block.data() + 32, nonce, NonceSize);
    for (std::size_t offset = 0; offset < out.size(); offset += 32) {
        const std::uint32_t counter = static_cast<std::uint32_t>(offset / 32);
        block[32 + NonceSize + 0] = static_cast<std::uint8_t>(counter & 0xFF);
        block[32 + NonceSize + 1] = static_cast<std::uint8_t>((counter >> 8) & 0xFF);
        block[32 + NonceSize + 2] = static_cast<std::uint8_t>((counter >> 16) & 0xFF);
        block[32 + NonceSize + 3] = static_cast<std::uint8_t>((counter >> 24) & 0xFF);
        const auto digest = pksm::crypto::sha256({block.data(), block.size()});
        const std::size_t remaining = std::min<std::size_t>(32, out.size() - offset);
        for (std::size_t i = 0; i < remaining; ++i) {
            out[offset + i] ^= digest[i];
        }
    }
}

std::array<std::uint8_t, TagSize> tag(
    const std::array<std::uint8_t, 32>& key,
    const std::uint8_t* nonce,
    const std::uint8_t* ciphertext,
    std::size_t length
) {
    std::vector<std::uint8_t> input;
    input.reserve(32 + 5 + NonceSize + length);
    input.insert(input.end(), key.begin(), key.end());
    const char label[] = "auth";
    input.insert(input.end(), label, label + 4);
    input.insert(input.end(), nonce, nonce + NonceSize);
    input.insert(input.end(), ciphertext, ciphertext + length);
    const auto digest = pksm::crypto::sha256({input.data(), input.size()});
    std::array<std::uint8_t, TagSize> result{};
    std::memcpy(result.data(), digest.data(), TagSize);
    return result;
}
}

bool CredentialStore::init() {
    if (R_FAILED(psInit())) {
        Logger::instance().error("psInit failed for credential store");
        return false;
    }
    Result result = PS_GetDeviceId(&deviceId_);
    if (R_FAILED(result) || deviceId_ == 0) {
        Logger::instance().error("PS_GetDeviceId failed: " + std::to_string(result));
        psExit();
        deviceId_ = 0;
        return false;
    }
    ::mkdir("sdmc:/3ds", 0777);
    ::mkdir("sdmc:/3ds/ReBank", 0777);
    return true;
}

std::string CredentialStore::path() {
    return "sdmc:/3ds/ReBank/creds.bin";
}

bool CredentialStore::save(const StoredCredentials& credentials) const {
    if (!available()) {
        return false;
    }
    if (credentials.username.empty() || credentials.password.empty()
        || credentials.username.size() > 32 || credentials.password.size() > 128) {
        return false;
    }
    std::vector<std::uint8_t> plaintext;
    plaintext.reserve(2 + credentials.username.size() + credentials.password.size());
    plaintext.push_back(static_cast<std::uint8_t>(credentials.username.size()));
    plaintext.insert(plaintext.end(), credentials.username.begin(), credentials.username.end());
    plaintext.push_back(static_cast<std::uint8_t>(credentials.password.size()));
    plaintext.insert(plaintext.end(), credentials.password.begin(), credentials.password.end());

    std::array<std::uint8_t, NonceSize> nonce{};
    if (R_FAILED(PS_GenerateRandomBytes(nonce.data(), nonce.size()))) {
        Logger::instance().error("PS_GenerateRandomBytes failed");
        return false;
    }
    const auto key = deriveKey(deviceId_);
    std::vector<std::uint8_t> ciphertext = plaintext;
    keystream(key, nonce.data(), ciphertext);
    const auto authTag = tag(key, nonce.data(), ciphertext.data(), ciphertext.size());

    FILE* file = std::fopen(path().c_str(), "wb");
    if (!file) {
        Logger::instance().error("credential save: open failed");
        return false;
    }
    std::fwrite(Magic.data(), 1, Magic.size(), file);
    std::fwrite(nonce.data(), 1, nonce.size(), file);
    const std::uint32_t length = static_cast<std::uint32_t>(ciphertext.size());
    std::fwrite(&length, 1, sizeof(length), file);
    std::fwrite(ciphertext.data(), 1, ciphertext.size(), file);
    std::fwrite(authTag.data(), 1, authTag.size(), file);
    std::fclose(file);
    return true;
}

std::optional<StoredCredentials> CredentialStore::load() const {
    if (!available()) {
        return std::nullopt;
    }
    FILE* file = std::fopen(path().c_str(), "rb");
    if (!file) {
        return std::nullopt;
    }
    std::array<char, 4> magic{};
    std::array<std::uint8_t, NonceSize> nonce{};
    std::uint32_t length = 0;
    std::array<std::uint8_t, TagSize> storedTag{};
    if (std::fread(magic.data(), 1, magic.size(), file) != magic.size()
        || std::fread(nonce.data(), 1, nonce.size(), file) != nonce.size()
        || std::fread(&length, 1, sizeof(length), file) != sizeof(length)
        || magic != Magic || length == 0 || length > 512) {
        std::fclose(file);
        return std::nullopt;
    }
    std::vector<std::uint8_t> ciphertext(length);
    if (std::fread(ciphertext.data(), 1, length, file) != length
        || std::fread(storedTag.data(), 1, storedTag.size(), file) != storedTag.size()) {
        std::fclose(file);
        return std::nullopt;
    }
    std::fclose(file);

    const auto key = deriveKey(deviceId_);
    const auto expected = tag(key, nonce.data(), ciphertext.data(), ciphertext.size());
    if (expected != storedTag) {
        Logger::instance().error("credential load: tag mismatch");
        return std::nullopt;
    }
    keystream(key, nonce.data(), ciphertext);

    if (ciphertext.size() < 2) {
        return std::nullopt;
    }
    const std::size_t usernameLen = ciphertext[0];
    if (1 + usernameLen + 1 > ciphertext.size()) {
        return std::nullopt;
    }
    const std::size_t passwordLen = ciphertext[1 + usernameLen];
    if (1 + usernameLen + 1 + passwordLen > ciphertext.size()) {
        return std::nullopt;
    }
    StoredCredentials result;
    result.username.assign(reinterpret_cast<char*>(ciphertext.data() + 1), usernameLen);
    result.password.assign(reinterpret_cast<char*>(ciphertext.data() + 1 + usernameLen + 1), passwordLen);
    return result;
}

bool CredentialStore::clear() const {
    return std::remove(path().c_str()) == 0;
}
