#pragma once

#include <enums/Ability.hpp>
#include <enums/GameVersion.hpp>
#include <enums/Gender.hpp>
#include <enums/Language.hpp>
#include <enums/Move.hpp>
#include <enums/Nature.hpp>
#include <enums/Type.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct PokemonSummary {
    std::uint16_t species = 0;
    std::uint16_t form = 0;
    std::uint8_t level = 0;
    bool shiny = false;
    std::uint16_t heldItem = 0;
    std::string nickname;
    std::string trainerName;
    std::string gameCode;
    std::uint8_t format = 0;
    pksm::Type type1 = pksm::Type::Normal;
    pksm::Type type2 = pksm::Type::Normal;
    pksm::GameVersion originGame;
    pksm::Language language = pksm::Language::None;
    std::array<pksm::Move, 4> moves{};
    pksm::Ability ability;
    pksm::Nature nature;
    pksm::Gender gender = pksm::Gender::Genderless;
};

struct PokemonPayload {
    std::uint8_t format = 0;
    std::vector<std::uint8_t> data;
};

struct BoxRead {
    std::array<PokemonSummary, 30> summaries{};
    std::array<PokemonPayload, 30> payloads{};
};
