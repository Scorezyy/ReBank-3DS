#pragma once

#include "save/catalog/GameCatalog.hpp"

#include <cstdint>
#include <memory>

namespace pksm {
class PKX;
class Sav;
class Generation;
}

namespace PokemonTransfer {
pksm::Generation expectedGeneration(PokemonFormat format);
std::uint8_t pokemonFormat(pksm::Generation generation);

pksm::Generation generationFromFormat(std::uint8_t format);

std::unique_ptr<pksm::PKX> convertForSave(
    const pksm::PKX& source,
    std::uint8_t sourceFormat,
    std::uint8_t targetGeneration,
    pksm::Sav& save
);
}
