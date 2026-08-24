#pragma once

#include "save/GameCatalog.hpp"

#include <cstdint>
#include <memory>

namespace pksm {
class PKX;
class Sav;
class Generation;
}

// Cross-generation Pokemon conversion for the local save's box writer.
// Pokemon may only move to an equal or newer generation - downward
// conversion is intentionally unsupported, since it is the source of lost
// data and bad eggs. A mon that reached Gen 6 stays Gen 6 and can no longer
// be placed into a Gen 4/5 save.
namespace PokemonTransfer {
pksm::Generation expectedGeneration(PokemonFormat format);
std::uint8_t pokemonFormat(pksm::Generation generation);
// Inverse of pokemonFormat(): returns Generation::UNUSED for anything
// outside the 4-7 range this app supports.
pksm::Generation generationFromFormat(std::uint8_t format);

// Returns null if sourceFormat >= targetGeneration (no conversion needed or
// unsupported downward move) or if the specific conversion path isn't
// implemented.
std::unique_ptr<pksm::PKX> convertForSave(
    const pksm::PKX& source,
    std::uint8_t sourceFormat,
    std::uint8_t targetGeneration,
    pksm::Sav& save
);
}
