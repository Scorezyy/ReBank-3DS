#include "selection/SelectionController.hpp"

#include "core/Logger.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace {
TextId modeLabel(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Row:
            return TextId::ModeRow;
        case SelectionMode::Area:
            return TextId::ModeArea;
        case SelectionMode::Single:
            break;
    }
    return TextId::ModeSingle;
}

SelectionMode nextMode(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Single:
            return SelectionMode::Row;
        case SelectionMode::Row:
            return SelectionMode::Area;
        case SelectionMode::Area:
            break;
    }
    return SelectionMode::Single;
}

std::string describeAddress(StorageAddress address, const BankSession& session) {
    if (address.isTrashCan()) {
        return "trash";
    }
    if (address.isCloudBank()) {
        return "bank " + std::to_string(session.cloudBox + 1);
    }
    if (address.pane == StoragePane::Party) {
        return "party";
    }
    return "local box " + std::to_string(session.localBox + 1);
}

HeldPokemon heldAt(StorageAddress address, std::size_t slot, std::size_t localBox, std::uint16_t cloudBox,
                   PokemonSummary summary, PokemonPayload payload) {
    return {address, slot, localBox, cloudBox, std::move(summary), std::move(payload)};
}
}

bool SelectionController::engaged() const {
    return state().phase != SelectionPhase::Idle;
}

bool SelectionController::holding() const {
    return state().phase == SelectionPhase::Holding;
}

bool SelectionController::marking() const {
    return state().phase == SelectionPhase::Marking;
}

SelectionMode SelectionController::mode() const {
    return state().mode;
}

void SelectionController::cycleMode() {
    if (engaged() || session_.hand.active) {
        return;
    }
    state().mode = nextMode(state().mode);
    context_.status = context_.text.get(modeLabel(state().mode));
}

void SelectionController::confirm() {
    switch (state().phase) {
        case SelectionPhase::Idle:
            beginSelection();
            return;
        case SelectionPhase::Marking:
            grab(session_.grid(session_.storagePane)
                     .rectangleSlots(state().anchorSlot, session_.focusedSlot));
            return;
        case SelectionPhase::Holding:
            place();
            return;
    }
}

void SelectionController::beginSelection() {
    if (state().mode == SelectionMode::Row) {
        const GridGeometry grid = session_.grid(session_.storagePane);
        grab(grid.rowSlots(grid.pointOf(session_.focusedSlot).row));
        return;
    }
    if (state().mode != SelectionMode::Area) {
        return;
    }
    state().anchorSlot = session_.focusedSlot;
    state().phase = SelectionPhase::Marking;
    context_.status = context_.text.get(TextId::MarkArea);
}

void SelectionController::cancel() {
    if (holding()) {
        restore();
    }
    context_.status = context_.text.get(TextId::SelectionCancelled);
    finish();
}

void SelectionController::finish() {
    state().region.clear();
    state().phase = SelectionPhase::Idle;
    state().anchorSlot = 0;
}

void SelectionController::focusOnRegion() {
    session_.focusedSlot = state().region.focusSlot();
}

void SelectionController::followBoxChange() {
    if (!holding()) {
        return;
    }
    state().region.setLocation(session_.focusedAddress());
    focusOnRegion();
}

void SelectionController::retryPlacement() {
    if (!holding()) {
        return;
    }
    place();
}

void SelectionController::move(CursorDirection direction) {
    if (!holding() || !mover_.move(state().region, direction)) {
        return;
    }
    const StorageAddress location = state().region.location();
    session_.storagePane = location.pane;
    if (location.isCloudBank() && session_.trashBoxActive) {
        session_.setTrashActive(false);
        storage_.refreshCloudBox();
    }
    focusOnRegion();
}

bool SelectionController::canGrab(StorageAddress address, const std::vector<std::size_t>& originSlots) {
    const std::string where = describeAddress(address, session_);
    if (!slots_.loaded(address)) {
        context_.status = context_.text.get(TextId::BankBoxLoading);
        Logger::instance().warning("selection grab blocked: " + where + " is still loading");
        return false;
    }
    const auto occupied = [&](std::size_t slot) { return slots_.occupied(address, slot); };
    if (std::none_of(originSlots.begin(), originSlots.end(), occupied)) {
        context_.status = context_.text.get(TextId::NothingToSelect);
        Logger::instance().info("selection grab blocked: " + where + " region has no Pokemon");
        return false;
    }
    const auto removedFromParty = static_cast<std::size_t>(std::count_if(originSlots.begin(), originSlots.end(), occupied));
    if (address.pane == StoragePane::Party && removedFromParty >= session_.party.occupiedCount()) {
        context_.status = context_.text.get(TextId::TeamCannotBeEmpty);
        Logger::instance().info("selection grab blocked: would empty the party");
        return false;
    }
    return true;
}

std::vector<RegionEntry> SelectionController::takeEntries(StorageAddress address,
                                                          const std::vector<std::size_t>& originSlots,
                                                          GridPoint anchor) {
    const GridGeometry grid = session_.grid(address.pane);
    std::vector<RegionEntry> entries;
    entries.reserve(originSlots.size());
    for (const std::size_t slot : originSlots) {
        SlotContents contents;
        const SlotState slotState = slots_.read(address, slot, contents);
        const GridPoint point = grid.pointOf(slot);
        RegionEntry& entry = entries.emplace_back();
        entry.originSlot = slot;
        entry.offset = GridPoint{point.row - anchor.row, point.column - anchor.column};
        entry.summary = std::move(contents.summary);
        entry.payload = std::move(contents.payload);
        entry.payloadKnown = slotState != SlotState::PayloadPending;
        slots_.clear(address, slot);
    }
    return entries;
}

void SelectionController::grab(const std::vector<std::size_t>& originSlots) {
    const StorageAddress address = session_.focusedAddress();
    if (!canGrab(address, originSlots)) {
        return;
    }
    const GridPoint anchor = session_.grid(address.pane).pointOf(originSlots.front());
    const RegionOrigin origin{address, session_.localBox, session_.cloudPosition(), session_.focusedSlot};
    state().region.adopt(takeEntries(address, originSlots, anchor), anchor, origin, session_.saveAdapter.boxCapacity());
    state().phase = SelectionPhase::Holding;
    state().anchorSlot = 0;
    storage_.persistDrafts();
    focusOnRegion();

    Logger::instance().info("selection grab: " + describeAddress(address, session_) + ", "
                            + std::to_string(state().region.size()) + " slots, "
                            + std::to_string(state().region.occupantCount()) + " Pokemon, "
                            + std::to_string(state().region.pendingPayloadCount()) + " awaiting payload");
    context_.status = context_.text.get(state().mode == SelectionMode::Row ? TextId::RowPickedUp : TextId::AreaPickedUp);
}

SelectionController::Placement SelectionController::placeEntry(const RegionEntry& entry, StorageAddress destination,
                                                               bool checkFormat) {
    if (!entry.summary.occupied()) {
        return Placement::Placed;
    }
    if (!entry.payloadKnown) {
        return entry.fetchAttempts >= RegionClipboard::MaxFetchAttempts ? Placement::Stalled : Placement::Pending;
    }
    const std::size_t slot = state().region.slotOf(entry);
    if (!slots_.available(destination, slot) || slots_.occupied(destination, slot)) {
        return Placement::Occupied;
    }
    if (checkFormat && destination.isInSave() && !session_.saveAdapter.canImportPokemon(entry.payload)) {
        return Placement::WrongFormat;
    }
    slots_.write(destination, slot, {entry.summary, entry.payload});
    state().region.recordPlacement({slots_.held(destination, slot, {entry.summary, entry.payload}), entry.originSlot});
    return Placement::Placed;
}

void SelectionController::place() {
    RegionClipboard& region = state().region;
    const StorageAddress destination = region.location();
    if (!slots_.loaded(destination)) {
        context_.status = context_.text.get(TextId::BankBoxLoading);
        Logger::instance().warning("selection place blocked: " + describeAddress(destination, session_)
                                   + " is still loading");
        return;
    }

    const bool checkFormat = destination != region.origin().address;
    std::array<std::size_t, static_cast<std::size_t>(Placement::Count)> counts{};
    std::vector<RegionEntry> kept;
    for (const RegionEntry& entry : region.entries()) {
        const Placement result = placeEntry(entry, destination, checkFormat);
        ++counts[static_cast<std::size_t>(result)];
        if (result != Placement::Placed) {
            kept.push_back(entry);
        }
    }
    const auto count = [&](Placement placement) { return counts[static_cast<std::size_t>(placement)]; };

    storage_.persistDrafts();
    Logger::instance().info("selection place: " + describeAddress(destination, session_) + ", placed "
                            + std::to_string(count(Placement::Placed)) + ", occupied "
                            + std::to_string(count(Placement::Occupied)) + ", wrong format "
                            + std::to_string(count(Placement::WrongFormat)) + ", pending "
                            + std::to_string(count(Placement::Pending)) + ", stalled "
                            + std::to_string(count(Placement::Stalled)));

    if (kept.empty()) {
        context_.status = context_.text.get(TextId::PlacedAll);
        finish();
        return;
    }

    region.retain(std::move(kept));
    focusOnRegion();
    constexpr std::pair<Placement, TextId> Reasons[] = {
        {Placement::Pending, TextId::PlaceReasonFetching},
        {Placement::Stalled, TextId::PlaceReasonUnavailable},
        {Placement::Occupied, TextId::PlaceReasonOccupied},
        {Placement::WrongFormat, TextId::PlaceReasonWrongFormat},
    };
    std::string reason;
    for (const auto& [placement, text] : Reasons) {
        if (count(placement) > 0) {
            reason += (reason.empty() ? "" : ", ") + context_.text.format(text, {std::to_string(count(placement))});
        }
    }
    context_.status = context_.text.format(count(Placement::Placed) > 0 ? TextId::PlacedSome : TextId::PlacedNone,
                                           {reason});
}

void SelectionController::restore() {
    const RegionClipboard& region = state().region;
    const RegionOrigin& origin = region.origin();
    storage_.persistDrafts();
    for (const RegionPlacement& placement : region.placements()) {
        HeldPokemon vacated = placement.destination;
        vacated.summary = {};
        vacated.payload = {};
        storage_.restorePokemon(vacated);
    }
    for (const RegionPlacement& placement : region.placements()) {
        storage_.restorePokemon(heldAt(origin.address, placement.originSlot, origin.localBox, origin.cloudBox,
                                       placement.destination.summary, placement.destination.payload));
    }
    for (const RegionEntry& entry : region.entries()) {
        storage_.restorePokemon(heldAt(origin.address, entry.originSlot, origin.localBox, origin.cloudBox,
                                       entry.summary, entry.payload));
    }
    storage_.persistDrafts();
    Logger::instance().info("selection restore: " + describeAddress(origin.address, session_) + ", "
                            + std::to_string(region.size()) + " held, "
                            + std::to_string(region.placements().size()) + " placed slots undone");
}

std::optional<PokemonSummary> SelectionController::heldSummaryAt(StorageAddress address,
                                                                 std::size_t slot) const {
    if (!holding()) {
        return std::nullopt;
    }
    return state().region.summaryAt(address, slot);
}

std::vector<std::size_t> SelectionController::markedSlots() const {
    if (!marking()) {
        return {};
    }
    return session_.grid(session_.storagePane)
        .rectangleSlots(state().anchorSlot, session_.focusedSlot);
}

std::optional<RegionPayloadRequest> SelectionController::nextPayloadRequest() const {
    const RegionClipboard& region = state().region;
    if (!holding() || !region.origin().address.isCloudBank()) {
        return std::nullopt;
    }
    const std::optional<std::size_t> slot = region.nextSlotAwaitingPayload();
    if (!slot) {
        return std::nullopt;
    }
    const auto entry = std::find_if(region.entries().begin(), region.entries().end(),
        [&](const RegionEntry& candidate) { return candidate.originSlot == *slot; });
    return RegionPayloadRequest{*slot, region.origin().cloudBox, entry->summary};
}

RegionClipboard* SelectionController::regionAwaitingPayloadsFrom(std::uint16_t cloudBox) {
    RegionClipboard& region = state().region;
    const bool matches = holding() && region.origin().address.isCloudBank() && region.origin().cloudBox == cloudBox;
    return matches ? &region : nullptr;
}

bool SelectionController::deliverPayload(std::uint16_t cloudBox, std::size_t slot, PokemonPayload payload) {
    RegionClipboard* region = regionAwaitingPayloadsFrom(cloudBox);
    return region && region->deliverPayload(slot, std::move(payload));
}

bool SelectionController::failPayload(std::uint16_t cloudBox, std::size_t slot) {
    RegionClipboard* region = regionAwaitingPayloadsFrom(cloudBox);
    return region && region->failPayload(slot);
}
