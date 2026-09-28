#pragma once

#include <string>
#include <string_view>

namespace AppPaths {
inline constexpr std::string_view Root = "sdmc:/3ds/ReBank";

inline std::string file(std::string_view relative) {
    return std::string(Root) + "/" + std::string(relative);
}

void ensureDirectory(std::string_view relative = {});
}
