#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

class DeviceIdentity {
public:
    using Key = std::array<std::uint8_t, 32>;

    void resolve();

    const std::string& fingerprint() const { return fingerprint_; }
    const std::optional<Key>& storageKey() const { return storageKey_; }

private:
    std::string fingerprint_;
    std::optional<Key> storageKey_;
};
