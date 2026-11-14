#include "plugin/PluginGames.hpp"

#include "platform/FileSystem.hpp"
#include "save/storage/TitleIds.hpp"

#include <cstdio>

namespace {
constexpr std::string_view PluginFolder = "sdmc:/luma/plugins/";
constexpr char HeaderMagic[] = "3GX$0002";
constexpr std::size_t MagicSize = 8;

constexpr std::array<std::uint64_t, 1> PokemonX = {TitleIds::require("x")};
constexpr std::array<std::uint64_t, 1> PokemonY = {TitleIds::require("y")};
constexpr std::array<std::uint64_t, 1> OmegaRuby = {TitleIds::require("omega-ruby")};
constexpr std::array<std::uint64_t, 1> AlphaSapphire = {TitleIds::require("alpha-sapphire")};
constexpr std::array<std::uint64_t, 1> PokemonSun = {TitleIds::require("sun")};
constexpr std::array<std::uint64_t, 1> PokemonMoon = {TitleIds::require("moon")};
constexpr std::array<std::uint64_t, 1> UltraSun = {TitleIds::require("ultra-sun")};
constexpr std::array<std::uint64_t, 1> UltraMoon = {TitleIds::require("ultra-moon")};
constexpr std::array<std::uint64_t, 7> AnimalCrossing = {0x0004000000086200ULL, 0x0004000000086300ULL,
    0x0004000000086400ULL, 0x0004000000086500ULL, 0x0004000000198D00ULL, 0x0004000000198E00ULL,
    0x0004000000198F00ULL};

constexpr std::array<PluginGame, 9> Games = {{
    {TextId::PluginGameX, PluginKind::LiveBox, PokemonX},
    {TextId::PluginGameSun, PluginKind::LiveBox, PokemonSun},
    {TextId::PluginGameY, PluginKind::LiveBox, PokemonY},
    {TextId::PluginGameMoon, PluginKind::LiveBox, PokemonMoon},
    {TextId::PluginGameOmegaRuby, PluginKind::LiveBox, OmegaRuby},
    {TextId::PluginGameUltraSun, PluginKind::LiveBox, UltraSun},
    {TextId::PluginGameAlphaSapphire, PluginKind::LiveBox, AlphaSapphire},
    {TextId::PluginGameUltraMoon, PluginKind::LiveBox, UltraMoon},
    {TextId::PluginGameAnimalCrossing, PluginKind::Acnl, AnimalCrossing},
}};

std::optional<std::string> headerVersion(const std::string& path) {
    if (!FileSystem::exists(path)) {
        return std::nullopt;
    }
    std::array<std::uint8_t, MagicSize + 4> header{};
    if (!FileSystem::readAt(path, 0, header) || std::string_view(reinterpret_cast<const char*>(header.data()), MagicSize) != std::string_view(HeaderMagic, MagicSize)) {
        return std::string();
    }
    return std::to_string(header[MagicSize + 3]) + "." + std::to_string(header[MagicSize + 2]) + "." + std::to_string(header[MagicSize + 1]);
}
}

std::span<const PluginGame> PluginGames::all() {
    return Games;
}

std::string_view PluginGames::catalogId(PluginKind kind) {
    return kind == PluginKind::Acnl ? "acnl" : "livebox";
}

std::string_view PluginGames::fileName(PluginKind kind) {
    return kind == PluginKind::Acnl ? "ReBank-ACNL.3gx" : "ReBank-LivePlugin.3gx";
}

std::string PluginGames::folderName(std::uint64_t titleId) {
    char name[17]{};
    std::snprintf(name, sizeof(name), "%016llX", static_cast<unsigned long long>(titleId));
    return name;
}

std::optional<std::string> PluginGames::installedVersion(const PluginGame& game) {
    for (const std::uint64_t titleId : game.titleIds) {
        const std::string path = std::string(PluginFolder) + folderName(titleId) + "/" + std::string(fileName(game.kind));
        if (std::optional<std::string> version = headerVersion(path)) {
            return version;
        }
    }
    return std::nullopt;
}
