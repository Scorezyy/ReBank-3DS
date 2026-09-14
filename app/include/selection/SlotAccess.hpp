#pragma once

#include "bank/BankSession.hpp"
#include "selection/StorageAddress.hpp"

#include <cstddef>
#include <cstdint>

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

    StorageAddress currentAddress() const;
    bool loaded(StorageAddress address) const;

    SlotState read(StorageAddress address, std::size_t slot, SlotContents& contents) const;
    PokemonSummary peek(StorageAddress address, std::size_t slot) const;
    bool occupied(StorageAddress address, std::size_t slot) const;

    void write(StorageAddress address, std::size_t slot, SlotContents contents);
    void clear(StorageAddress address, std::size_t slot);

private:
    SlotState readCloudBank(std::size_t slot, SlotContents& contents) const;
    void writeCloudBank(std::size_t slot, SlotContents contents);

    BankSession& session_;
};
