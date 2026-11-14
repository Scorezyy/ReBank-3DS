#include "save/storage/TitleIds.hpp"

#include <algorithm>

namespace TitleIds {
std::optional<std::uint64_t> titleIdFor(std::string_view code) {
    const auto mapping = std::find_if(TitleMappings.begin(), TitleMappings.end(),
        [&](const TitleMapping& item) { return item.code == code; });
    return mapping == TitleMappings.end() ? std::nullopt : std::optional(mapping->titleId);
}

std::optional<std::string_view> codeFor(std::uint64_t titleId) {
    const auto mapping = std::find_if(TitleMappings.begin(), TitleMappings.end(),
        [&](const TitleMapping& item) { return item.titleId == titleId; });
    return mapping == TitleMappings.end() ? std::nullopt : std::optional(mapping->code);
}
}
