#include "save/PokemonTransfer.hpp"

#include <pkx/PK5.hpp>
#include <pkx/PK6.hpp>
#include <pkx/PK7.hpp>
#include <pkx/PKX.hpp>
#include <sav/Sav.hpp>

namespace PokemonTransfer {

pksm::Generation expectedGeneration(PokemonFormat format) {
    switch (format) {
        case PokemonFormat::Generation4:
            return pksm::Generation::FOUR;
        case PokemonFormat::Generation5:
            return pksm::Generation::FIVE;
        case PokemonFormat::Generation6:
            return pksm::Generation::SIX;
        case PokemonFormat::Generation7:
            return pksm::Generation::SEVEN;
    }
    return pksm::Generation::UNUSED;
}

std::uint8_t pokemonFormat(pksm::Generation generation) {
    if (generation == pksm::Generation::FOUR) {
        return 4;
    }
    if (generation == pksm::Generation::FIVE) {
        return 5;
    }
    if (generation == pksm::Generation::SIX) {
        return 6;
    }
    if (generation == pksm::Generation::SEVEN) {
        return 7;
    }
    return 0;
}

pksm::Generation generationFromFormat(std::uint8_t format) {
    switch (format) {
        case 4: return pksm::Generation::FOUR;
        case 5: return pksm::Generation::FIVE;
        case 6: return pksm::Generation::SIX;
        case 7: return pksm::Generation::SEVEN;
        default: return pksm::Generation::UNUSED;
    }
}

// Delegates to PKSM-Core's own PKX::convertToG5/G6/G7 - the exact same
// conversion machinery PKSM itself uses for its Bank. Each override already
// chains through the intermediate generations correctly (e.g. a Gen 4 mon
// going to Gen 6 is internally converted 4->5->6, matching how a real
// Pal Park + Poke Transporter transfer works), so this only needs to pick
// the right top-level call.
std::unique_ptr<pksm::PKX> convertForSave(
    const pksm::PKX& source,
    std::uint8_t sourceFormat,
    std::uint8_t targetGeneration,
    pksm::Sav& save
) {
    if (sourceFormat >= targetGeneration) {
        return nullptr;
    }
    switch (targetGeneration) {
        case 5: return source.convertToG5(save);
        case 6: return source.convertToG6(save);
        case 7: return source.convertToG7(save);
        default: return nullptr;
    }
}

}
