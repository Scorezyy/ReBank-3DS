#pragma once

#include <string>

namespace Hmac {
std::string sha256Hex(const std::string& data);
std::string sha256HmacHex(const std::string& key, const std::string& message);
}
