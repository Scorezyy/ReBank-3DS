#include "GameCatalog.hpp"

#include <array>

namespace {
constexpr std::array Games{
    GameDescriptor{"diamond", "Pokemon Diamond", GamePlatform::NintendoDs, PokemonFormat::Generation4},
    GameDescriptor{"pearl", "Pokemon Pearl", GamePlatform::NintendoDs, PokemonFormat::Generation4},
    GameDescriptor{"platinum", "Pokemon Platinum", GamePlatform::NintendoDs, PokemonFormat::Generation4},
    GameDescriptor{"heartgold", "Pokemon HeartGold", GamePlatform::NintendoDs, PokemonFormat::Generation4},
    GameDescriptor{"soulsilver", "Pokemon SoulSilver", GamePlatform::NintendoDs, PokemonFormat::Generation4},
    GameDescriptor{"black", "Pokemon Black", GamePlatform::NintendoDs, PokemonFormat::Generation5},
    GameDescriptor{"white", "Pokemon White", GamePlatform::NintendoDs, PokemonFormat::Generation5},
    GameDescriptor{"black2", "Pokemon Black 2", GamePlatform::NintendoDs, PokemonFormat::Generation5},
    GameDescriptor{"white2", "Pokemon White 2", GamePlatform::NintendoDs, PokemonFormat::Generation5},
    GameDescriptor{"x", "Pokemon X", GamePlatform::Nintendo3Ds, PokemonFormat::Generation6},
    GameDescriptor{"y", "Pokemon Y", GamePlatform::Nintendo3Ds, PokemonFormat::Generation6},
    GameDescriptor{"omega-ruby", "Pokemon Omega Ruby", GamePlatform::Nintendo3Ds, PokemonFormat::Generation6},
    GameDescriptor{"alpha-sapphire", "Pokemon Alpha Sapphire", GamePlatform::Nintendo3Ds, PokemonFormat::Generation6},
    GameDescriptor{"sun", "Pokemon Sun", GamePlatform::Nintendo3Ds, PokemonFormat::Generation7},
    GameDescriptor{"moon", "Pokemon Moon", GamePlatform::Nintendo3Ds, PokemonFormat::Generation7},
    GameDescriptor{"ultra-sun", "Pokemon Ultra Sun", GamePlatform::Nintendo3Ds, PokemonFormat::Generation7},
    GameDescriptor{"ultra-moon", "Pokemon Ultra Moon", GamePlatform::Nintendo3Ds, PokemonFormat::Generation7}
};
}

std::span<const GameDescriptor> supportedGames() {
    return Games;
}