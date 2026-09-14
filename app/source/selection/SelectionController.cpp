#include "selection/SelectionController.hpp"

#include "app/App.hpp"
#include "core/Logger.hpp"

#include <utility>

namespace {
const char* modeLabel(SelectionMode mode) {
    switch (mode) {
        case SelectionMode::Row:
            return "Row selection mode.";
        case SelectionMode::Area:
            return "Area selection mode.";
        case SelectionMode::Single:
            break;
    }
    return "Single selection mode.";
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
    app_.status_ = modeLabel(state().mode);
}

void SelectionController::confirm() {
    switch (state().phase) {
        case SelectionPhase::Idle:
            beginSelection();
            return;
        case SelectionPhase::Marking:
            grab(GridGeometry::forPane(session_.storagePane)
                     .rectangleSlots(state().anchorSlot, session_.focusedSlot));
            return;
        case SelectionPhase::Holding:
            place();
            return;
    }
}

void SelectionController::beginSelection() {
    if (state().mode == SelectionMode::Row) {
        const GridGeometry grid = GridGeometry::forPane(session_.storagePane);
        grab(grid.rowSlots(grid.pointOf(session_.focusedSlot).row));
        return;
    }
    if (state().mode != SelectionMode::Area) {
        return;
    }
    state().anchorSlot = session_.focusedSlot;
    state().phase = SelectionPhase::Marking;
    app_.status_ = "Mark the area, then press A.";
}

void SelectionController::cancel() {
    if (holding()) {
        restore();
    }
    app_.status_ = "Selection cancelled.";
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
    state().region.setLocation(slots_.currentAddress());
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
        session_.trashBoxActive = false;
        session_.trashTransitionStart = svcGetSystemTick();
        storage_.refreshCloudBox();
    }
    focusOnRegion();
}

bool SelectionController::anyOccupied(StorageAddress address, const std::vector<std::size_t>& slots) const {
    for (std::size_t slot : slots) {
        if (slots_.occupied(address, slot)) {
            return true;
        }
    }
    return false;
}

bool SelectionController::wouldEmptyParty(const std::vector<std::size_t>& slots) const {
    int remaining = session_.partyMemberCount();
    for (std::size_t slot : slots) {
        if (session_.partyWorking.summaries[slot].species != 0) {
            --remaining;
        }
    }
    return remaining <= 0;
}

bool SelectionController::acceptedBySave(const PokemonPayload& payload) const {
    return session_.saveAdapter.canImportPokemon(payload.format, payload.data);
}

void SelectionController::grab(const std::vector<std::size_t>& originSlots) {
    const StorageAddress address = slots_.currentAddress();
    if (!slots_.loaded(address)) {
        app_.status_ = "This bank box is still loading.";
        Logger::instance().warning("selection grab blocked: " + describeAddress(address, session_)
                                   + " is still loading");
        return;
    }
    if (!anyOccupied(address, originSlots)) {
        app_.status_ = "Nothing to select here.";
        Logger::instance().info("selection grab blocked: " + describeAddress(address, session_)
                                + " region has no Pokemon");
        return;
    }
    if (address.pane == StoragePane::Party && wouldEmptyParty(originSlots)) {
        app_.status_ = "Your team can't be empty.";
        Logger::instance().info("selection grab blocked: would empty the party");
        return;
    }

    const GridGeometry grid = GridGeometry::forPane(address.pane);
    const GridPoint anchor = grid.pointOf(originSlots.front());

    std::vector<RegionEntry> entries;
    entries.reserve(originSlots.size());
    for (std::size_t slot : originSlots) {
        SlotContents contents;
        const SlotState slotState = slots_.read(address, slot, contents);
        const GridPoint point = grid.pointOf(slot);
        RegionEntry entry;
        entry.originSlot = slot;
        entry.offset = GridPoint{point.row - anchor.row, point.column - anchor.column};
        entry.summary = std::move(contents.summary);
        entry.payload = std::move(contents.payload);
        entry.payloadKnown = slotState != SlotState::PayloadPending;
        entries.push_back(std::move(entry));
    }

    RegionOrigin origin;
    origin.address = address;
    origin.localBox = session_.localBox;
    origin.cloudBox = static_cast<std::uint16_t>(session_.cloudBox + 1);
    origin.focusedSlot = session_.focusedSlot;

    for (std::size_t slot : originSlots) {
        slots_.clear(address, slot);
    }

    state().region.adopt(std::move(entries), anchor, origin);
    state().phase = SelectionPhase::Holding;
    state().anchorSlot = 0;
    storage_.persistDrafts();
    focusOnRegion();

    Logger::instance().info("selection grab: " + describeAddress(address, session_) + ", "
                            + std::to_string(state().region.size()) + " slots, "
                            + std::to_string(state().region.occupantCount()) + " Pokemon, "
                            + std::to_string(state().region.pendingPayloadCount()) + " awaiting payload");
    app_.status_ = state().mode == SelectionMode::Row ? "Row picked up." : "Area picked up.";
}

void SelectionController::place() {
    RegionClipboard& region = state().region;
    const StorageAddress destination = region.location();
    if (!slots_.loaded(destination)) {
        app_.status_ = "This bank box is still loading.";
        Logger::instance().warning("selection place blocked: " + describeAddress(destination, session_)
                                   + " is still loading");
        return;
    }

    const bool checkFormat = destination != region.origin().address;
    const bool intoSave = destination.pane == StoragePane::Local || destination.pane == StoragePane::Party;

    std::size_t placed = 0;
    std::size_t occupied = 0;
    std::size_t wrongFormat = 0;
    std::size_t pending = 0;
    std::size_t stalled = 0;
    std::vector<RegionEntry> kept;
    for (const RegionEntry& entry : region.entries()) {
        if (entry.summary.species == 0) {
            ++placed;
            continue;
        }
        const std::size_t slot = region.slotOf(entry);
        if (!entry.payloadKnown) {
            if (entry.fetchAttempts >= RegionClipboard::MaxFetchAttempts) {
                ++stalled;
            } else {
                ++pending;
            }
            kept.push_back(entry);
            continue;
        }
        if (slots_.occupied(destination, slot)) {
            ++occupied;
            kept.push_back(entry);
            continue;
        }
        if (checkFormat && intoSave && !acceptedBySave(entry.payload)) {
            ++wrongFormat;
            kept.push_back(entry);
            continue;
        }
        slots_.write(destination, slot, SlotContents{entry.summary, entry.payload});
        ++placed;
    }

    storage_.persistDrafts();
    Logger::instance().info("selection place: " + describeAddress(destination, session_) + ", placed "
                            + std::to_string(placed) + ", occupied " + std::to_string(occupied)
                            + ", wrong format " + std::to_string(wrongFormat) + ", pending "
                            + std::to_string(pending) + ", stalled " + std::to_string(stalled));

    if (kept.empty()) {
        app_.status_ = "Placed.";
        finish();
        return;
    }

    region.retain(std::move(kept));
    focusOnRegion();
    std::string reason;
    if (pending > 0) {
        reason += "fetching " + std::to_string(pending);
    }
    if (stalled > 0) {
        reason += (reason.empty() ? "" : ", ") + std::to_string(stalled) + " unavailable from the server";
    }
    if (occupied > 0) {
        reason += (reason.empty() ? "" : ", ") + std::to_string(occupied) + " spot(s) occupied";
    }
    if (wrongFormat > 0) {
        reason += (reason.empty() ? "" : ", ") + std::to_string(wrongFormat) + " can't enter this save";
    }
    app_.status_ = (placed > 0 ? "Placed what fit - " : "Nothing placed - ") + reason + ".";
}

void SelectionController::restore() {
    const RegionClipboard& region = state().region;
    const RegionOrigin& origin = region.origin();
    for (const RegionEntry& entry : region.entries()) {
        storage_.restorePokemon(handSourceOf(origin.address), entry.originSlot, origin.localBox,
                                origin.cloudBox, origin.address.trash, entry.summary, entry.payload);
    }
    storage_.persistDrafts();
    Logger::instance().info("selection restore: " + describeAddress(origin.address, session_) + ", "
                            + std::to_string(region.size()) + " slots");
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
    return GridGeometry::forPane(session_.storagePane)
        .rectangleSlots(state().anchorSlot, session_.focusedSlot);
}

std::optional<RegionPayloadRequest> SelectionController::nextPayloadRequest() const {
    if (!holding()) {
        return std::nullopt;
    }
    const RegionClipboard& region = state().region;
    if (!region.origin().address.isCloudBank()) {
        return std::nullopt;
    }
    const std::optional<std::size_t> slot = region.nextSlotAwaitingPayload();
    if (!slot) {
        return std::nullopt;
    }
    RegionPayloadRequest request;
    request.slot = *slot;
    request.cloudBox = region.origin().cloudBox;
    for (const RegionEntry& entry : region.entries()) {
        if (entry.originSlot == *slot) {
            request.summary = entry.summary;
            break;
        }
    }
    return request;
}

bool SelectionController::deliverPayload(std::uint16_t cloudBox, std::size_t slot, PokemonPayload payload) {
    if (!holding()) {
        return false;
    }
    RegionClipboard& region = state().region;
    if (!region.origin().address.isCloudBank() || region.origin().cloudBox != cloudBox) {
        return false;
    }
    return region.deliverPayload(slot, std::move(payload));
}

bool SelectionController::failPayload(std::uint16_t cloudBox, std::size_t slot) {
    if (!holding()) {
        return false;
    }
    RegionClipboard& region = state().region;
    if (!region.origin().address.isCloudBank() || region.origin().cloudBox != cloudBox) {
        return false;
    }
    return region.failPayload(slot);
}
