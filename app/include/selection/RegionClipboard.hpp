#pragma once

#include "bank/BankTypes.hpp"
#include "selection/GridGeometry.hpp"
#include "selection/StorageAddress.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct RegionEntry {
    std::size_t originSlot = 0;
    GridPoint offset;
    PokemonSummary summary;
    PokemonPayload payload;
    bool payloadKnown = false;
    std::uint8_t fetchAttempts = 0;
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

    void adopt(std::vector<RegionEntry> entries, GridPoint anchor, RegionOrigin origin);
    void retain(std::vector<RegionEntry> remaining);
    void clear();

    const RegionOrigin& origin() const { return origin_; }
    StorageAddress location() const { return location_; }
    GridPoint anchor() const { return anchor_; }
    void moveTo(StorageAddress address, GridPoint anchor);
    void setLocation(StorageAddress address);

    GridGeometry grid() const { return GridGeometry::forPane(location_.pane); }
    GridPoint minOffset() const { return minOffset_; }
    GridPoint maxOffset() const { return maxOffset_; }
    GridPoint span() const;

    std::size_t slotOf(const RegionEntry& entry) const;
    std::optional<PokemonSummary> summaryAt(StorageAddress address, std::size_t slot) const;
    std::size_t focusSlot() const;

    std::optional<std::size_t> nextSlotAwaitingPayload() const;
    std::size_t pendingPayloadCount() const;
    std::size_t stalledPayloadCount() const;
    std::size_t occupantCount() const;
    bool deliverPayload(std::size_t originSlot, PokemonPayload payload);
    bool failPayload(std::size_t originSlot);

private:
    void recomputeExtent();

    std::vector<RegionEntry> entries_;
    RegionOrigin origin_;
    StorageAddress location_;
    GridPoint anchor_;
    GridPoint minOffset_;
    GridPoint maxOffset_;
};
