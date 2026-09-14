#include "selection/RegionClipboard.hpp"

#include <algorithm>
#include <utility>

void RegionClipboard::adopt(std::vector<RegionEntry> entries, GridPoint anchor, RegionOrigin origin) {
    entries_ = std::move(entries);
    origin_ = origin;
    location_ = origin.address;
    anchor_ = anchor;
    recomputeExtent();
}

void RegionClipboard::retain(std::vector<RegionEntry> remaining) {
    entries_ = std::move(remaining);
    recomputeExtent();
}

void RegionClipboard::clear() {
    entries_.clear();
    origin_ = RegionOrigin{};
    location_ = StorageAddress{};
    anchor_ = GridPoint{};
    minOffset_ = GridPoint{};
    maxOffset_ = GridPoint{};
}

void RegionClipboard::moveTo(StorageAddress address, GridPoint anchor) {
    location_ = address;
    anchor_ = anchor;
}

void RegionClipboard::setLocation(StorageAddress address) {
    location_ = address;
}

GridPoint RegionClipboard::span() const {
    return GridPoint{maxOffset_.row - minOffset_.row + 1, maxOffset_.column - minOffset_.column + 1};
}

void RegionClipboard::recomputeExtent() {
    if (entries_.empty()) {
        minOffset_ = GridPoint{};
        maxOffset_ = GridPoint{};
        return;
    }
    minOffset_ = entries_.front().offset;
    maxOffset_ = entries_.front().offset;
    for (const RegionEntry& entry : entries_) {
        minOffset_.row = std::min(minOffset_.row, entry.offset.row);
        minOffset_.column = std::min(minOffset_.column, entry.offset.column);
        maxOffset_.row = std::max(maxOffset_.row, entry.offset.row);
        maxOffset_.column = std::max(maxOffset_.column, entry.offset.column);
    }
}

std::size_t RegionClipboard::slotOf(const RegionEntry& entry) const {
    return grid().slotOf(GridPoint{anchor_.row + entry.offset.row, anchor_.column + entry.offset.column});
}

std::optional<PokemonSummary> RegionClipboard::summaryAt(StorageAddress address, std::size_t slot) const {
    if (entries_.empty() || address != location_) {
        return std::nullopt;
    }
    for (const RegionEntry& entry : entries_) {
        if (slotOf(entry) == slot) {
            return entry.summary;
        }
    }
    return std::nullopt;
}

std::size_t RegionClipboard::focusSlot() const {
    if (entries_.empty()) {
        return 0;
    }
    for (const RegionEntry& entry : entries_) {
        if (entry.originSlot == origin_.focusedSlot) {
            return slotOf(entry);
        }
    }
    return slotOf(entries_.front());
}

std::optional<std::size_t> RegionClipboard::nextSlotAwaitingPayload() const {
    for (const RegionEntry& entry : entries_) {
        if (entry.summary.species != 0 && !entry.payloadKnown && entry.fetchAttempts < MaxFetchAttempts) {
            return entry.originSlot;
        }
    }
    return std::nullopt;
}

std::size_t RegionClipboard::pendingPayloadCount() const {
    std::size_t pending = 0;
    for (const RegionEntry& entry : entries_) {
        if (entry.summary.species != 0 && !entry.payloadKnown && entry.fetchAttempts < MaxFetchAttempts) {
            ++pending;
        }
    }
    return pending;
}

std::size_t RegionClipboard::stalledPayloadCount() const {
    std::size_t stalled = 0;
    for (const RegionEntry& entry : entries_) {
        if (entry.summary.species != 0 && !entry.payloadKnown && entry.fetchAttempts >= MaxFetchAttempts) {
            ++stalled;
        }
    }
    return stalled;
}

std::size_t RegionClipboard::occupantCount() const {
    std::size_t occupants = 0;
    for (const RegionEntry& entry : entries_) {
        if (entry.summary.species != 0) {
            ++occupants;
        }
    }
    return occupants;
}

bool RegionClipboard::deliverPayload(std::size_t originSlot, PokemonPayload payload) {
    for (RegionEntry& entry : entries_) {
        if (entry.originSlot != originSlot || entry.payloadKnown) {
            continue;
        }
        entry.payload = std::move(payload);
        entry.payloadKnown = true;
        return true;
    }
    return false;
}

bool RegionClipboard::failPayload(std::size_t originSlot) {
    for (RegionEntry& entry : entries_) {
        if (entry.originSlot != originSlot || entry.payloadKnown) {
            continue;
        }
        if (entry.fetchAttempts < MaxFetchAttempts) {
            ++entry.fetchAttempts;
        }
        return true;
    }
    return false;
}
