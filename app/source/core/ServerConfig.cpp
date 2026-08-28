#include "core/ServerConfig.hpp"

#ifndef REBANK_SERVER_SCHEME
#error REBANK_SERVER_SCHEME must be defined by config/server.mk
#endif

#ifndef REBANK_SERVER_HOST
#error REBANK_SERVER_HOST must be defined by config/server.mk
#endif

#ifndef REBANK_SERVER_PORT
#error REBANK_SERVER_PORT must be defined by config/server.mk
#endif

#ifndef REBANK_CLIENT_SECRET
#error REBANK_CLIENT_SECRET must be defined by config/client-secret.mk (copy config/client-secret.mk.example and fill in a real secret)
#endif

std::string_view ServerConfig::scheme() {
    return REBANK_SERVER_SCHEME;
}

std::string_view ServerConfig::host() {
    return REBANK_SERVER_HOST;
}

std::uint16_t ServerConfig::port() {
    return REBANK_SERVER_PORT;
}

std::string ServerConfig::baseUrl() {
    return std::string(scheme()) + "://" + std::string(host()) + ":" + std::to_string(port());
}

std::string_view ServerConfig::clientSecret() {
    return REBANK_CLIENT_SECRET;
}