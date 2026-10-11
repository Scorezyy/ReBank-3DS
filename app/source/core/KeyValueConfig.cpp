#include "core/KeyValueConfig.hpp"

#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"

#include <cstdio>

namespace {
std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t'
        || text.back() == '\r' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}

std::optional<std::string> valueForKey(std::string_view line, std::string_view key) {
    const auto separator = line.find('=');
    if (separator == std::string_view::npos || trimmed(line.substr(0, separator)) != key) {
        return std::nullopt;
    }
    return std::string(trimmed(line.substr(separator + 1)));
}
}

namespace KeyValueConfig {
std::optional<std::string> lookup(std::string_view fileName, std::string_view key) {
    const FsGuard guard;
    FILE* file = std::fopen(AppPaths::file(fileName).c_str(), "r");
    if (!file) {
        return std::nullopt;
    }
    std::optional<std::string> found;
    char line[256];
    while (!found && std::fgets(line, sizeof(line), file)) {
        found = valueForKey(line, key);
    }
    std::fclose(file);
    return found;
}
}
