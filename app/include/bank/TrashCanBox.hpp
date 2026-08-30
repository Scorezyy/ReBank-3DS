#pragma once

#include "bank/BankTypes.hpp"

#include <array>
#include <cstddef>

class TrashCanBox {
public:
    static constexpr std::size_t SlotCount = 30;

    std::array<PokemonSummary, SlotCount>& summaries() { return summaries_; }
    const std::array<PokemonSummary, SlotCount>& summaries() const { return summaries_; }
    std::array<PokemonPayload, SlotCount>& payloads() { return payloads_; }
    const std::array<PokemonPayload, SlotCount>& payloads() const { return payloads_; }

    bool empty() const;
    std::size_t count() const;
    void reset();

private:
    std::array<PokemonSummary, SlotCount> summaries_{};
    std::array<PokemonPayload, SlotCount> payloads_{};
};
