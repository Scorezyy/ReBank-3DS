#pragma once

#include "gui/icons/IconPixels.hpp"

#include <citro2d.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

struct InstalledTitles {
    std::vector<std::uint64_t> sd;
    std::uint64_t cartridge = 0;

    static InstalledTitles scan();
    std::optional<std::pair<std::uint64_t, FS_MediaType>> find(const std::span<const std::uint64_t>& titleIds) const;
};

namespace TitleIcon {
std::optional<IconPixels> read(const std::span<const std::uint64_t>& titleIds, const InstalledTitles& installed);
}
