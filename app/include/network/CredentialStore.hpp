#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct StoredCredentials {
    std::string username;
    std::string password;
};

// Persists credentials on the SD card encrypted with a key derived from this console's device ID.
class CredentialStore {
public:
    bool init();
    bool save(const StoredCredentials& credentials) const;
    std::optional<StoredCredentials> load() const;
    bool clear() const;
    bool available() const { return deviceId_ != 0; }

private:
    static std::string path();
    std::uint32_t deviceId_ = 0;
};
