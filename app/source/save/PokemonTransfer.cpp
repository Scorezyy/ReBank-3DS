#include "save/PokemonTransfer.hpp"
#include "core/Logger.hpp"

#include <pkx/PKX.hpp>
#include <pkx/PK4.hpp>
#include <pkx/PK5.hpp>
#include <pkx/PK6.hpp>
#include <pkx/PK7.hpp>
#include <sav/Sav.hpp>
#include <utils/DateTime.hpp>

#include <algorithm>
#include <array>

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

namespace {

bool isGen4Hm(pksm::Move move) {
    return move == pksm::Move::Cut || move == pksm::Move::Fly
        || move == pksm::Move::Surf || move == pksm::Move::Strength
        || move == pksm::Move::Whirlpool || move == pksm::Move::RockSmash
        || move == pksm::Move::Waterfall || move == pksm::Move::RockClimb;
}

std::uint16_t gen4TransferLocation(const pksm::PKX& pokemon) {
    if (!pokemon.fatefulEncounter()) {
        return 30001;
    }
    switch (pokemon.species()) {
        case pksm::Species::Raikou:
        case pksm::Species::Entei:
        case pksm::Species::Suicune:
            return 30012;
        case pksm::Species::Celebi:
            return 30010;
        default:
            return 30001;
    }
}

void copyTransferMoves(const pksm::PKX& source, pksm::PKX& target, bool removeGen4Hms) {
    std::uint8_t destination = 0;
    for (std::uint8_t index = 0; index < 4; ++index) {
        const pksm::Move move = source.move(index);
        if (move == pksm::Move::None || (removeGen4Hms && isGen4Hm(move))) {
            continue;
        }
        target.move(destination, move);
        target.PPUp(destination, std::min<std::uint8_t>(source.PPUp(index), 3));
        target.PP(destination, source.PP(index));
        ++destination;
    }
    while (destination < 4) {
        target.move(destination, pksm::Move::None);
        target.PPUp(destination, 0);
        target.PP(destination, 0);
        ++destination;
    }
}

std::unique_ptr<pksm::PKX> convertG4ToG5Stable(const pksm::PKX& source) {
    if (source.generation() != pksm::Generation::FOUR) {
        return nullptr;
    }
    std::array<std::uint8_t, pksm::PK5::BOX_LENGTH> raw{};
    const auto sourceRaw = source.rawData();
    std::copy_n(sourceRaw.begin(), std::min(raw.size(), sourceRaw.size()), raw.begin());
    auto target = pksm::PKX::getPKM<pksm::Generation::FIVE>(
        raw.data(), raw.size());
    if (!target) {
        return nullptr;
    }

    const std::uint32_t originalPid = source.PID();
    auto& pk5 = static_cast<pksm::PK5&>(*target);
    std::fill(target->rawData().begin() + 0x42, target->rawData().begin() + 0x48, 0);
    std::fill(target->rawData().begin() + 0x86, target->rawData().begin() + 0x88, 0);
    target->heldItem(0);
    target->otFriendship(70);
    target->nature(source.nature());
    target->metDate(Date::today());
    target->metLocation(gen4TransferLocation(source));
    target->metLevel(source.level());
    target->nickname(source.nickname());
    target->otName(source.otName());
    target->ball(source.ball());
    pk5.hiddenAbility(false);
    pk5.nPokemon(false);
    copyTransferMoves(source, *target, true);
    target->PID(originalPid);
    target->refreshChecksum();
    if (target->PID() != originalPid) {
        Logger::instance().error("Gen 4 to Gen 5 conversion changed PID");
        return nullptr;
    }
    Logger::instance().info("Gen 4 to Gen 5: PID preserved "
                            + std::to_string(originalPid));
    return target;
}

std::uint8_t gen6AbilitySlot(
    const pksm::PKX& pokemon,
    pksm::Ability ability,
    bool hiddenAbility
) {
    if (hiddenAbility && ability == pokemon.abilities(2)) {
        return 4;
    }
    if (ability == pokemon.abilities(0)) {
        return 1;
    }
    if (ability == pokemon.abilities(1)) {
        return 2;
    }
    if (ability == pokemon.abilities(2)) {
        return 4;
    }
    return hiddenAbility ? 4 : 1;
}

std::unique_ptr<pksm::PKX> convertLegacyToG6Stable(const pksm::PKX& source, pksm::Sav& save) {
    auto target = pksm::PKX::getPKM<pksm::Generation::SIX>(nullptr, pksm::PK6::BOX_LENGTH);
    Logger::instance().info("Legacy to Gen 6: target allocated");
    const auto species = static_cast<std::uint16_t>(source.species());
    const bool fromGen4 = source.generation() == pksm::Generation::FOUR;
    const bool fromGen5 = source.generation() == pksm::Generation::FIVE;
    if (!target || (!fromGen4 && !fromGen5) || species == 0
        || species > (fromGen4 ? 493 : 649)) {
        return nullptr;
    }

    target->encryptionConstant(source.PID());
    target->species(source.species());
    target->heldItem(0);
    target->TID(source.TID());
    target->SID(source.SID());
    target->experience(source.experience());
    target->PID(source.PID());
    target->ability(source.ability());
    target->nature(source.nature());
    target->fatefulEncounter(source.fatefulEncounter());
    target->gender(source.gender());
    target->alternativeForm(source.alternativeForm());
    const auto sourceAbility = source.ability();
    const bool hiddenAbility = fromGen5 && source.abilityNumber() == 4;
    const std::uint8_t abilitySlot = gen6AbilitySlot(*target, sourceAbility, hiddenAbility);
    if (sourceAbility != target->abilities(0)
        && sourceAbility != target->abilities(1)
        && sourceAbility != target->abilities(2)) {
        Logger::instance().warning("Legacy to Gen 6: ability ID not present in Gen 6 personal data");
    }
    target->abilityNumber(abilitySlot);
    target->egg(source.egg());
    target->nicknamed(true);
    target->nickname(source.nickname());
    target->otName(source.otName());
    target->otGender(source.otGender());
    target->version(source.version());
    target->ball(source.ball());
    target->metLevel(fromGen4 ? source.level() : source.metLevel());
    target->metLocation(fromGen4 ? 30001 : source.metLocation());
    target->metDate(fromGen4 ? Date::today() : source.metDate());
    target->eggLocation(fromGen4 ? 0 : source.eggLocation());
    target->pkrsDays(source.pkrsDays());
    target->pkrsStrain(source.pkrsStrain());
    auto& pk6 = static_cast<pksm::PK6&>(*target);
    pk6.encounterType(fromGen4
        ? static_cast<const pksm::PK4&>(source).encounterType()
        : static_cast<const pksm::PK5&>(source).encounterType());

    const auto languageValue = static_cast<std::uint8_t>(source.language());
    target->language(languageValue >= 1 && languageValue <= 8 && languageValue != 6
        ? source.language()
        : pksm::Language::ENG);
    Logger::instance().info("Legacy to Gen 6: identity copied");

    for (std::uint8_t index = 0; index < 6; ++index) {
        const auto stat = pksm::Stat(index);
        target->ev(stat, std::min<std::uint16_t>(source.ev(stat), 252));
        target->iv(stat, source.iv(stat));
        target->contest(index, source.contest(index));
    }
    copyTransferMoves(source, *target, fromGen4);
    Logger::instance().info("Legacy to Gen 6: stats and moves copied");

    target->currentHandler(pksm::PKXHandler::NonOT);
    pk6.htName(save.otName());
    pk6.htGender(save.gender());
    pk6.htFriendship(70);
    pk6.region(save.subRegion());
    pk6.country(save.country());
    pk6.consoleRegion(save.consoleRegion());
    target->refreshChecksum();
    Logger::instance().info(
        "Legacy to Gen 6: ready abilitySlot=" + std::to_string(target->abilityNumber())
        + " metLevel=" + std::to_string(target->metLevel())
        + " metLocation=" + std::to_string(target->metLocation())
        + " encounterType=" + std::to_string(pk6.encounterType()));
    return target;
}

}

std::unique_ptr<pksm::PKX> convertForSave(
    const pksm::PKX& source,
    std::uint8_t sourceFormat,
    std::uint8_t targetGeneration,
    pksm::Sav& save
) {
    if (sourceFormat >= targetGeneration) {
        return nullptr;
    }
    if (sourceFormat == 4 && targetGeneration == 5) {
        return convertG4ToG5Stable(source);
    }
    if ((sourceFormat == 4 || sourceFormat == 5) && targetGeneration == 6) {
        return convertLegacyToG6Stable(source, save);
    }
    if ((sourceFormat == 4 || sourceFormat == 5) && targetGeneration == 7) {
        auto gen6 = convertLegacyToG6Stable(source, save);
        return gen6 ? gen6->convertToG7(save) : nullptr;
    }
    if (sourceFormat == 6 && targetGeneration == 7) {
        return source.convertToG7(save);
    }
    return nullptr;
}

}
