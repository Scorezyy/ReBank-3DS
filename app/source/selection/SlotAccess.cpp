#include "selection/SlotAccess.hpp"

#include <utility>

StorageAddress SlotAccess::currentAddress() const {
    StorageAddress address;
    address.pane = session_.storagePane;
    address.trash = session_.storagePane == StoragePane::Cloud && session_.trashBoxActive;
    return address;
}

bool SlotAccess::loaded(StorageAddress address) const {
    if (!address.isCloudBank()) {
        return true;
    }
    return !session_.cloudViewAwaitingLoad;
}

SlotState SlotAccess::readCloudBank(std::size_t slot, SlotContents& contents) const {
    contents.summary = session_.cloudPreview[slot];
    if (contents.summary.species == 0) {
        return SlotState::Empty;
    }
    if (!session_.pendingUploadPayloads[slot].data.empty()) {
        contents.payload = session_.pendingUploadPayloads[slot];
        return SlotState::Ready;
    }
    if (!session_.cachedCloudPayloads[slot].data.empty()) {
        contents.payload = session_.cachedCloudPayloads[slot];
        return SlotState::Ready;
    }
    return SlotState::PayloadPending;
}

SlotState SlotAccess::read(StorageAddress address, std::size_t slot, SlotContents& contents) const {
    if (address.isCloudBank()) {
        return readCloudBank(slot, contents);
    }
    if (address.isTrashCan()) {
        contents.summary = session_.trashBox.summaries()[slot];
        contents.payload = session_.trashBox.payloads()[slot];
    } else if (address.pane == StoragePane::Party) {
        contents.summary = session_.partyWorking.summaries[slot];
        contents.payload = session_.partyWorking.payloads[slot];
    } else {
        contents.summary = session_.storage.pokemon(slot);
        contents.payload = session_.localPayloads[slot];
    }
    return contents.summary.species == 0 ? SlotState::Empty : SlotState::Ready;
}

PokemonSummary SlotAccess::peek(StorageAddress address, std::size_t slot) const {
    if (address.isCloudBank()) {
        return session_.cloudPreview[slot];
    }
    if (address.isTrashCan()) {
        return session_.trashBox.summaries()[slot];
    }
    if (address.pane == StoragePane::Party) {
        return session_.partyWorking.summaries[slot];
    }
    return session_.storage.pokemon(slot);
}

bool SlotAccess::occupied(StorageAddress address, std::size_t slot) const {
    return peek(address, slot).species != 0;
}

void SlotAccess::writeCloudBank(std::size_t slot, SlotContents contents) {
    session_.cloudPreview[slot] = std::move(contents.summary);
    session_.pendingUploadPayloads[slot] = std::move(contents.payload);
    session_.cachedCloudPayloads[slot] = {};
    session_.payloadPrefetchFailed[slot] = false;
}

void SlotAccess::write(StorageAddress address, std::size_t slot, SlotContents contents) {
    if (address.isCloudBank()) {
        writeCloudBank(slot, std::move(contents));
        return;
    }
    if (address.isTrashCan()) {
        session_.trashBox.summaries()[slot] = std::move(contents.summary);
        session_.trashBox.payloads()[slot] = std::move(contents.payload);
        return;
    }
    if (address.pane == StoragePane::Party) {
        session_.partyWorking.summaries[slot] = std::move(contents.summary);
        session_.partyWorking.payloads[slot] = std::move(contents.payload);
        return;
    }
    session_.storage.set(slot, contents.summary);
    session_.localPayloads[slot] = std::move(contents.payload);
}

void SlotAccess::clear(StorageAddress address, std::size_t slot) {
    write(address, slot, SlotContents{});
}
