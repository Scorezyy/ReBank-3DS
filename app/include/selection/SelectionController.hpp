#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/StorageController.hpp"
#include "selection/CursorDirection.hpp"
#include "selection/RegionMover.hpp"
#include "selection/SelectionState.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct RegionPayloadRequest {
    std::size_t slot = 0;
    std::uint16_t cloudBox = 0;
    PokemonSummary summary;
};

class SelectionController {
public:
    SelectionController(BankContext& context, BankSession& session, StorageController& storage)
        : context_(context), session_(session), storage_(storage), slots_(storage.slots()) {}

    bool engaged() const;
    bool holding() const;
    bool marking() const;
    SelectionMode mode() const;

    void cycleMode();
    void confirm();
    void cancel();
    void move(CursorDirection direction);
    void followBoxChange();
    void retryPlacement();

    std::optional<PokemonSummary> heldSummaryAt(StorageAddress address, std::size_t slot) const;
    std::vector<std::size_t> markedSlots() const;

    std::optional<RegionPayloadRequest> nextPayloadRequest() const;
    bool deliverPayload(std::uint16_t cloudBox, std::size_t slot, PokemonPayload payload);
    bool failPayload(std::uint16_t cloudBox, std::size_t slot);

private:
    SelectionState& state() { return session_.selection; }
    const SelectionState& state() const { return session_.selection; }

    enum class Placement : std::uint8_t { Placed, Pending, Stalled, Occupied, WrongFormat, Count };

    void beginSelection();
    void grab(const std::vector<std::size_t>& originSlots);
    bool canGrab(StorageAddress address, const std::vector<std::size_t>& originSlots);
    std::vector<RegionEntry> takeEntries(StorageAddress address, const std::vector<std::size_t>& originSlots,
                                         GridPoint anchor);
    void place();
    Placement placeEntry(const RegionEntry& entry, StorageAddress destination, bool checkFormat);
    void restore();
    void finish();
    void focusOnRegion();
    RegionClipboard* regionAwaitingPayloadsFrom(std::uint16_t cloudBox);

    BankContext& context_;
    BankSession& session_;
    StorageController& storage_;
    SlotAccess& slots_;
    RegionMover mover_;
};
