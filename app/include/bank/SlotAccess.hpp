#pragma once

#include "bank/BankSession.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

enum class SlotState : std::uint8_t {
    Empty,
    Ready,
    PayloadPending
};

struct SlotContents {
    PokemonSummary summary;
    PokemonPayload payload;
};

class SlotAccess {
public:
    explicit SlotAccess(BankSession& session) : session_(session) {}

    bool loaded(StorageAddress address) const;
    bool available(StorageAddress address, std::size_t slot) const;
    SlotState read(StorageAddress address, std::size_t slot, SlotContents& contents) const;
    const PokemonSummary& peek(StorageAddress address, std::size_t slot) const;
    bool occupied(StorageAddress address, std::size_t slot) const { return peek(address, slot).occupied(); }

    void write(StorageAddress address, std::size_t slot, SlotContents contents);
    void clear(StorageAddress address, std::size_t slot) { write(address, slot, {}); }

    HeldPokemon held(StorageAddress address, std::size_t slot, SlotContents contents) const;
    std::string describe(StorageAddress address, std::size_t slot) const;

private:
    struct Columns {
        std::span<PokemonSummary> summaries;
        std::span<PokemonPayload> payloads;
    };

    Columns columns(StorageAddress address) const;

    BankSession& session_;
};
