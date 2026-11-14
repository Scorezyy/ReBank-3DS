#pragma once

#include "i18n/TextId.hpp"

#include <array>
#include <cstddef>
#include <string_view>

using Translation = std::array<std::string_view, static_cast<std::size_t>(TextId::Count)>;

constexpr std::size_t at(TextId id) {
    return static_cast<std::size_t>(id);
}

namespace Translations {
const Translation& english();
const Translation& german();
const Translation& french();
const Translation& spanish();
const Translation& italian();
const Translation& portuguese();
}
