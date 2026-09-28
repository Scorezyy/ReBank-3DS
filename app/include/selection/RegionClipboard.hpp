#pragma once

#include "bank/BankTypes.hpp"
#include "selection/GridGeometry.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

struct RegionEntry {
    std::size_t originSlot = 0;
    GridPoint offset;
    PokemonSummary summary;
    PokemonPayload payload;
    bool payloadKnown = false;
    std::uint8_t fetchAttempts = 0;
};

struct RegionPlacement {
    HeldPokemon destination;
    std::size_t originSlot = 0;
};

struct RegionOrigin {
    StorageAddress address;
    std::size_t localBox = 0;
    std::uint16_t cloudBox = 0;
    std::size_t focusedSlot = 0;
};

class RegionClipboard {
public:
    static constexpr std::uint8_t MaxFetchAttempts = 3;

    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }
    const std::vector<RegionEntry>& entries() const { return entries_; }

    void adopt(std::vector<RegionEntry> entries, GridPoint anchor, RegionOrigin origin, std::size_t localCapacity);
    void retain(std::vector<RegionEntry> remaining);
    void clear();
    void recordPlacement(RegionPlacement placement) { placements_.push_back(std::move(placement)); }
    const std::vector<RegionPlacement>& placements() const { return placements_; }

    const RegionOrigin& origin() const { return origin_; }
    StorageAddress location() const { return location_; }
    GridPoint anchor() const { return anchor_; }
    void moveTo(StorageAddress address, GridPoint anchor);
    void setLocation(StorageAddress address);

    GridGeometry grid() const { return gridFor(location_.pane); }
    GridGeometry gridFor(StoragePane pane) const { return GridGeometry::forPane(pane, localCapacity_); }
    GridPoint minOffset() const { return minOffset_; }
    GridPoint maxOffset() const { return maxOffset_; }
    GridPoint span() const;

    std::size_t slotOf(const RegionEntry& entry) const;
    std::optional<PokemonSummary> summaryAt(StorageAddress address, std::size_t slot) const;
    std::size_t focusSlot() const;

    std::optional<std::size_t> nextSlotAwaitingPayload() const;
    std::size_t pendingPayloadCount() const;
    std::size_t occupantCount() const;
    bool deliverPayload(std::size_t originSlot, PokemonPayload payload);
    bool failPayload(std::size_t originSlot);

private:
    void recomputeExtent();

    std::vector<RegionEntry> entries_;
    std::vector<RegionPlacement> placements_;
    RegionOrigin origin_;
    StorageAddress location_;
    GridPoint anchor_;
    GridPoint minOffset_;
    GridPoint maxOffset_;
    std::size_t localCapacity_ = BoxSlotCount;
};
