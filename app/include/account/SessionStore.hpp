#pragma once

#include "account/DeviceIdentity.hpp"

#include <string>

class SessionStore {
public:
    explicit SessionStore(const DeviceIdentity& identity) : identity_(identity) {}

    bool load(std::string& refreshToken, std::string& username) const;
    bool save(const std::string& refreshToken, const std::string& username) const;
    void clear() const;
    static void removeLegacyCredentials();

private:
    const DeviceIdentity& identity_;
};
