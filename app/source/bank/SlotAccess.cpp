#include "bank/SlotAccess.hpp"

#include <utility>

SlotAccess::Columns SlotAccess::columns(StorageAddress address) const {
    if (address.isCloudBank()) {
        return {session_.cloud.summaries, session_.cloud.pending};
    }
    if (address.isTrashCan()) {
        return {session_.trash.summaries, session_.trash.payloads};
    }
    if (address.pane == StoragePane::Party) {
        return {session_.party.summaries, session_.party.payloads};
    }
    return {session_.local.summaries, session_.local.payloads};
}

bool SlotAccess::loaded(StorageAddress address) const {
    return !address.isCloudBank() || (!session_.cloud.awaitingLoad && session_.currentCloudDraft() != nullptr);
}

bool SlotAccess::available(StorageAddress address, std::size_t slot) const {
    return address.pane != StoragePane::Local || slot < session_.saveAdapter.boxCapacity();
}

SlotState SlotAccess::read(StorageAddress address, std::size_t slot, SlotContents& contents) const {
    const Columns view = columns(address);
    contents.summary = view.summaries[slot];
    contents.payload = view.payloads[slot];
    if (!contents.summary.occupied()) {
        return SlotState::Empty;
    }
    if (address.isCloudBank() && !contents.payload.known()) {
        contents.payload = session_.cloud.cached[slot];
    }
    return contents.payload.known() || !address.isCloudBank() ? SlotState::Ready : SlotState::PayloadPending;
}

const PokemonSummary& SlotAccess::peek(StorageAddress address, std::size_t slot) const {
    return columns(address).summaries[slot];
}

void SlotAccess::write(StorageAddress address, std::size_t slot, SlotContents contents) {
    const Columns view = columns(address);
    view.summaries[slot] = std::move(contents.summary);
    view.payloads[slot] = std::move(contents.payload);
    if (address.isCloudBank()) {
        session_.cloud.cached[slot] = {};
        session_.cloud.prefetchFailed[slot] = false;
    }
}

HeldPokemon SlotAccess::held(StorageAddress address, std::size_t slot, SlotContents contents) const {
    return {address, slot, session_.localBox, session_.cloudPosition(), std::move(contents.summary),
            std::move(contents.payload)};
}

std::string SlotAccess::describe(StorageAddress address, std::size_t slot) const {
    const std::string slotText = " slot " + std::to_string(slot + 1);
    if (address.isTrashCan()) {
        return "trash box" + slotText;
    }
    if (address.isCloudBank()) {
        return "bank " + std::to_string(session_.cloudBox + 1) + slotText;
    }
    if (address.pane == StoragePane::Party) {
        return "party" + slotText;
    }
    return "local box " + std::to_string(session_.localBox + 1) + slotText;
}
