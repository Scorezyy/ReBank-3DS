#include "update/PluginGames.hpp"

#include <cstdio>

namespace {
constexpr std::string_view PluginFolder = "sdmc:/luma/plugins/";
constexpr char HeaderMagic[] = "3GX$0002";
constexpr std::size_t MagicSize = 8;

constexpr std::array<std::string_view, 1> PokemonX = {"0004000000055D00"};
constexpr std::array<std::string_view, 1> PokemonY = {"0004000000055E00"};
constexpr std::array<std::string_view, 1> OmegaRuby = {"000400000011C400"};
constexpr std::array<std::string_view, 1> AlphaSapphire = {"000400000011C500"};
constexpr std::array<std::string_view, 1> PokemonSun = {"0004000000164800"};
constexpr std::array<std::string_view, 1> PokemonMoon = {"0004000000175E00"};
constexpr std::array<std::string_view, 1> UltraSun = {"00040000001B5000"};
constexpr std::array<std::string_view, 1> UltraMoon = {"00040000001B5100"};
constexpr std::array<std::string_view, 7> AnimalCrossing = {"0004000000086200", "0004000000086300", "0004000000086400",
    "0004000000086500", "0004000000198D00", "0004000000198E00", "0004000000198F00"};

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
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return std::nullopt;
    }
    unsigned char header[MagicSize + 4]{};
    const bool read = std::fread(header, 1, sizeof(header), file) == sizeof(header);
    std::fclose(file);
    if (!read || std::string_view(reinterpret_cast<const char*>(header), MagicSize) != std::string_view(HeaderMagic, MagicSize)) {
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

std::optional<std::string> PluginGames::installedVersion(const PluginGame& game) {
    for (const std::string_view folder : game.folders) {
        const std::string path = std::string(PluginFolder) + std::string(folder) + "/" + std::string(fileName(game.kind));
        if (std::optional<std::string> version = headerVersion(path)) {
            return version;
        }
    }
    return std::nullopt;
}
