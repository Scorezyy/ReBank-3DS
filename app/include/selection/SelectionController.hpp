#pragma once

#include "bank/BankSession.hpp"
#include "bank/StorageController.hpp"
#include "selection/CursorDirection.hpp"
#include "selection/RegionMover.hpp"
#include "selection/SelectionState.hpp"
#include "selection/SlotAccess.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class App;

struct RegionPayloadRequest {
    std::size_t slot = 0;
    std::uint16_t cloudBox = 0;
    PokemonSummary summary;
};

class SelectionController {
public:
    SelectionController(App& app, BankSession& session, StorageController& storage)
        : app_(app), session_(session), storage_(storage), slots_(session) {}

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

    void beginSelection();
    void grab(const std::vector<std::size_t>& originSlots);
    void place();
    void restore();
    void finish();
    void focusOnRegion();

    bool anyOccupied(StorageAddress address, const std::vector<std::size_t>& slots) const;
    bool wouldEmptyParty(const std::vector<std::size_t>& slots) const;
    bool acceptedBySave(const PokemonPayload& payload) const;

    App& app_;
    BankSession& session_;
    StorageController& storage_;
    SlotAccess slots_;
    RegionMover mover_;
};
