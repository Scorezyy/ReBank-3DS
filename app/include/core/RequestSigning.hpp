#pragma once

#include <string>
#include <string_view>

namespace RequestSigning {
std::string sign(
    std::string_view method,
    std::string_view path,
    std::string_view version,
    std::string_view timestamp,
    const std::string& body,
    std::string_view secret
);
}
