#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace KeyValueConfig {
std::optional<std::string> lookup(std::string_view fileName, std::string_view key);
}
