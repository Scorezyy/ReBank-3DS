#include "platform/KeyValueConfig.hpp"

#include "platform/AppPaths.hpp"
#include "platform/FileSystem.hpp"
#include "platform/FsGuard.hpp"

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
    for (const std::string& line : FileSystem::readLines(AppPaths::file(fileName))) {
        if (std::optional<std::string> value = valueForKey(line, key)) {
            return value;
        }
    }
    return std::nullopt;
}
}
