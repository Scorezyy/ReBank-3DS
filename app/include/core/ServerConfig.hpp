#pragma once

#include <cstdint>
#include <string>
#include <string_view>

class ServerConfig {
public:
    static std::string_view scheme();
    static std::string_view host();
    static std::uint16_t port();
    static std::string baseUrl();
};