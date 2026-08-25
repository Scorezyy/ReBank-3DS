#pragma once

#include <string>

class SessionStore {
public:
    bool load(std::string& refreshToken, std::string& username) const;
    bool save(const std::string& refreshToken, const std::string& username) const;
    void clear() const;
};