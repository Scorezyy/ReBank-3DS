#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace VirtualConsoleTitles {
std::optional<std::uint64_t> resolveInstalledTitleId(std::string_view code);
void resetInstalledCache();
}
