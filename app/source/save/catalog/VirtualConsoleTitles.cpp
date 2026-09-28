#include "save/catalog/VirtualConsoleTitles.hpp"

#include "core/AppPaths.hpp"
#include "core/FsGuard.hpp"

#include <3ds.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
struct KnownTitle {
    std::string_view code;
    std::uint64_t titleId;
};

constexpr std::array KnownTitles{
    KnownTitle{"green", 0x0004000000170D00ULL},
    KnownTitle{"red", 0x0004000000170C00ULL},
    KnownTitle{"red", 0x0004000000171000ULL},
    KnownTitle{"red", 0x0004000000171300ULL},
    KnownTitle{"red", 0x0004000000171600ULL},
    KnownTitle{"red", 0x0004000000171900ULL},
    KnownTitle{"red", 0x0004000000171C00ULL},
    KnownTitle{"blue", 0x0004000000170E00ULL},
    KnownTitle{"blue", 0x0004000000171100ULL},
    KnownTitle{"blue", 0x0004000000171400ULL},
    KnownTitle{"blue", 0x0004000000171700ULL},
    KnownTitle{"blue", 0x0004000000171A00ULL},
    KnownTitle{"blue", 0x0004000000171D00ULL},
    KnownTitle{"yellow", 0x0004000000170F00ULL},
    KnownTitle{"yellow", 0x0004000000171200ULL},
    KnownTitle{"yellow", 0x0004000000171500ULL},
    KnownTitle{"yellow", 0x0004000000171800ULL},
    KnownTitle{"yellow", 0x0004000000171B00ULL},
    KnownTitle{"yellow", 0x0004000000171E00ULL},
    KnownTitle{"gold", 0x0004000000172300ULL},
    KnownTitle{"gold", 0x0004000000172600ULL},
    KnownTitle{"gold", 0x0004000000172900ULL},
    KnownTitle{"gold", 0x0004000000172C00ULL},
    KnownTitle{"gold", 0x0004000000172F00ULL},
    KnownTitle{"gold", 0x0004000000173200ULL},
    KnownTitle{"gold", 0x0004000000173500ULL},
    KnownTitle{"silver", 0x0004000000172400ULL},
    KnownTitle{"silver", 0x0004000000172700ULL},
    KnownTitle{"silver", 0x0004000000172A00ULL},
    KnownTitle{"silver", 0x0004000000172D00ULL},
    KnownTitle{"silver", 0x0004000000173000ULL},
    KnownTitle{"silver", 0x0004000000173300ULL},
    KnownTitle{"silver", 0x0004000000173600ULL},
    KnownTitle{"crystal", 0x0004000000172500ULL},
    KnownTitle{"crystal", 0x0004000000172800ULL},
    KnownTitle{"crystal", 0x0004000000172B00ULL},
    KnownTitle{"crystal", 0x0004000000172E00ULL},
    KnownTitle{"crystal", 0x0004000000173100ULL},
    KnownTitle{"crystal", 0x0004000000173400ULL}
};

std::optional<std::vector<std::uint64_t>>& installedTitleCache() {
    static std::optional<std::vector<std::uint64_t>> cache;
    return cache;
}

std::vector<std::uint64_t> listInstalledTitles() {
    std::vector<std::uint64_t> titles;
    if (R_FAILED(amInit())) {
        return titles;
    }
    u32 count = 0;
    if (R_SUCCEEDED(AM_GetTitleCount(MEDIATYPE_SD, &count)) && count > 0) {
        titles.resize(count);
        u32 read = 0;
        titles.resize(R_SUCCEEDED(AM_GetTitleList(&read, MEDIATYPE_SD, count, titles.data())) ? read : 0);
    }
    amExit();
    return titles;
}

const std::vector<std::uint64_t>& installedTitles() {
    std::optional<std::vector<std::uint64_t>>& cache = installedTitleCache();
    if (!cache) {
        cache = listInstalledTitles();
    }
    return *cache;
}

bool isInstalled(std::uint64_t titleId) {
    const auto& titles = installedTitles();
    return std::find(titles.begin(), titles.end(), titleId) != titles.end();
}

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

std::optional<std::uint64_t> configuredTitleId(std::string_view code) {
    const FsGuard guard;
    FILE* file = std::fopen(AppPaths::file("vc_titles.cfg").c_str(), "r");
    if (!file) {
        return std::nullopt;
    }
    std::optional<std::uint64_t> found;
    char line[128];
    while (std::fgets(line, sizeof(line), file)) {
        const std::string_view text(line);
        const auto separator = text.find('=');
        if (separator == std::string_view::npos) {
            continue;
        }
        if (trimmed(text.substr(0, separator)) != code) {
            continue;
        }
        const std::string value(trimmed(text.substr(separator + 1)));
        found = std::strtoull(value.c_str(), nullptr, 16);
        break;
    }
    std::fclose(file);
    return found;
}
}

namespace VirtualConsoleTitles {

void resetInstalledCache() {
    installedTitleCache().reset();
}

std::optional<std::uint64_t> resolveInstalledTitleId(std::string_view code) {
    for (const KnownTitle& title : KnownTitles) {
        if (title.code == code && isInstalled(title.titleId)) {
            return title.titleId;
        }
    }
    if (const auto configured = configuredTitleId(code); configured && isInstalled(*configured)) {
        return configured;
    }
    return std::nullopt;
}

}
