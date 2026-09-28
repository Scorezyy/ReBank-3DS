#include "save/pokemon/PokemonTransfer.hpp"

#include <pkx/PK2.hpp>
#include <pkx/PK4.hpp>
#include <pkx/PK5.hpp>
#include <pkx/PK6.hpp>
#include <pkx/PK7.hpp>
#include <pkx/PKX.hpp>
#include <sav/Sav.hpp>

#include <array>

namespace PokemonTransfer {

namespace {
constexpr std::array<pksm::Generation, 8> GenerationsByFormat{
    pksm::Generation::UNUSED, pksm::Generation::ONE, pksm::Generation::TWO, pksm::Generation::THREE,
    pksm::Generation::FOUR, pksm::Generation::FIVE, pksm::Generation::SIX, pksm::Generation::SEVEN
};
}

std::uint8_t pokemonFormat(pksm::Generation generation) {
    for (std::size_t format = 1; format < GenerationsByFormat.size(); ++format) {
        if (GenerationsByFormat[format] == generation) {
            return static_cast<std::uint8_t>(format);
        }
    }
    return 0;
}

pksm::Generation generationFromFormat(std::uint8_t format) {
    return format < GenerationsByFormat.size() ? GenerationsByFormat[format] : pksm::Generation::UNUSED;
}

pksm::Generation expectedGeneration(PokemonFormat format) {
    return generationFromFormat(static_cast<std::uint8_t>(format));
}

bool canConvert(std::uint8_t sourceFormat, std::uint8_t targetFormat) {
    if (generationFromFormat(sourceFormat) == pksm::Generation::UNUSED
        || generationFromFormat(targetFormat) == pksm::Generation::UNUSED) {
        return false;
    }
    if (sourceFormat == targetFormat) {
        return true;
    }
    return sourceFormat < targetFormat && targetFormat != 1;
}

std::unique_ptr<pksm::PKX> convertForSave(
    const pksm::PKX& source,
    std::uint8_t sourceFormat,
    std::uint8_t targetGeneration,
    pksm::Sav& save
) {
    if (sourceFormat == targetGeneration || !canConvert(sourceFormat, targetGeneration)) {
        return nullptr;
    }
    std::unique_ptr<pksm::PKX> converted;
    switch (targetGeneration) {
        case 2: converted = source.convertToG2(save); break;
        case 4: converted = source.convertToG4(save); break;
        case 5: converted = source.convertToG5(save); break;
        case 6: converted = source.convertToG6(save); break;
        case 7: converted = source.convertToG7(save); break;
        default: return nullptr;
    }
    if (converted && !converted->nicknamed()) {
        converted->nickname(converted->species().localize(converted->language()));
        converted->refreshChecksum();
    }
    return converted;
}

}
