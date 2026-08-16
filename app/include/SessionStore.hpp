#pragma once

#include <string>

class SessionStore {
public:
    bool load(std::string& refreshToken) const;
    bool save(const std::string& refreshToken) const;
    void clear() const;
};