#pragma once

#include <enums/Ability.hpp>
#include <enums/GameVersion.hpp>
#include <enums/Gender.hpp>
#include <enums/Language.hpp>
#include <enums/Move.hpp>
#include <enums/Nature.hpp>
#include <enums/Type.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr std::size_t BoxSlotCount = 30;
inline constexpr std::size_t PartySlotCount = 6;

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

    bool occupied() const { return species != 0; }
};

struct PokemonPayload {
    std::uint8_t format = 0;
    std::vector<std::uint8_t> data;

    bool known() const { return !data.empty(); }
    friend bool operator==(const PokemonPayload&, const PokemonPayload&) = default;
};

inline bool sameIdentity(const PokemonSummary& a, const PokemonSummary& b) {
    return a.species == b.species && a.nickname == b.nickname;
}

inline bool sameSlotContent(const PokemonSummary& summaryA, const PokemonPayload& payloadA,
                            const PokemonSummary& summaryB, const PokemonPayload& payloadB) {
    return sameIdentity(summaryA, summaryB) && payloadA.data == payloadB.data;
}

template <std::size_t Count>
struct SlotGroup {
    std::array<PokemonSummary, Count> summaries{};
    std::array<PokemonPayload, Count> payloads{};

    bool slotDiffers(const SlotGroup& other, std::size_t slot) const {
        return !sameSlotContent(summaries[slot], payloads[slot], other.summaries[slot], other.payloads[slot]);
    }

    bool differs(const SlotGroup& other) const {
        for (std::size_t slot = 0; slot < Count; ++slot) {
            if (slotDiffers(other, slot)) {
                return true;
            }
        }
        return false;
    }

    std::size_t occupiedCount() const {
        std::size_t count = 0;
        for (const PokemonSummary& summary : summaries) {
            count += summary.occupied() ? 1 : 0;
        }
        return count;
    }

    bool empty() const { return occupiedCount() == 0; }
};

using BoxSlots = SlotGroup<BoxSlotCount>;
using PartySlots = SlotGroup<PartySlotCount>;
