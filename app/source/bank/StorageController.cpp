#include "bank/StorageController.hpp"

#include "platform/Logger.hpp"

#include <utility>

namespace {
std::string slotLocation(const char* area, std::size_t box, std::size_t slot) {
    return std::string(area) + " " + std::to_string(box + 1) + " slot " + std::to_string(slot + 1);
}

template <typename Predicate>
std::optional<std::size_t> firstSlotWhere(std::size_t count, Predicate&& predicate) {
    for (std::size_t slot = 0; slot < count; ++slot) {
        if (predicate(slot)) {
            return slot;
        }
    }
    return std::nullopt;
}
}

BoxSlots& StorageController::localDraftForWrite(std::size_t box) {
    if (const auto draft = session_.localDrafts.find(box); draft != session_.localDrafts.end()) {
        return draft->second;
    }
    const auto baseline = session_.localBaselines.find(box);
    BoxSlots draft = baseline != session_.localBaselines.end() ? baseline->second : session_.saveAdapter.readBox(box);
    session_.localBaselines.emplace(box, draft);
    return session_.localDrafts.emplace(box, std::move(draft)).first->second;
}

void StorageController::restorePokemon(const HeldPokemon& pokemon) {
    const std::size_t slot = pokemon.sourceIndex;
    if (pokemon.source.pane == StoragePane::Local && pokemon.sourceLocalBox != session_.localBox) {
        BoxSlots& draft = localDraftForWrite(pokemon.sourceLocalBox);
        draft.summaries[slot] = pokemon.summary;
        draft.payloads[slot] = pokemon.payload;
        return;
    }
    if (!pokemon.source.isCloudBank()) {
        slots_.write(pokemon.source, slot, {pokemon.summary, pokemon.payload});
        return;
    }
    if (pokemon.sourceCloudBox == 0) {
        return;
    }
    const auto boxKey = static_cast<std::uint16_t>(pokemon.sourceCloudBox - 1);
    const auto box = session_.cloudBoxes.find(boxKey);
    const bool backToBaseline = box != session_.cloudBoxes.end()
        && sameIdentity(box->second.baseline[slot], pokemon.summary)
        && box->second.payloads[slot].data == pokemon.payload.data;
    const PokemonPayload pending = backToBaseline ? PokemonPayload{} : pokemon.payload;
    if (box != session_.cloudBoxes.end()) {
        box->second.summaries[slot] = pokemon.summary;
        box->second.pending[slot] = pending;
    }
    if (boxKey == session_.cloudKey()) {
        session_.cloud.summaries[slot] = pokemon.summary;
        session_.cloud.pending[slot] = pending;
        session_.cloud.cached[slot] = backToBaseline ? pokemon.payload : PokemonPayload{};
    }
}

std::optional<std::string> StorageController::pendingChange() const {
    for (const auto check : {&StorageController::openLocalBoxChange, &StorageController::openCloudBoxChange,
                             &StorageController::partyChange, &StorageController::trashChange,
                             &StorageController::storedLocalDraftChange, &StorageController::storedCloudDraftChange}) {
        if (std::optional<std::string> change = (this->*check)()) {
            return change;
        }
    }
    return std::nullopt;
}

std::optional<std::string> StorageController::openLocalBoxChange() const {
    const auto baseline = session_.localBaselines.find(session_.localBox);
    if (baseline == session_.localBaselines.end()) {
        return std::nullopt;
    }
    const auto slot = firstSlotWhere(BoxSlotCount,
        [&](std::size_t index) { return session_.local.slotDiffers(baseline->second, index); });
    return slot ? std::optional(slotLocation("local box", session_.localBox, *slot)) : std::nullopt;
}

std::optional<std::string> StorageController::openCloudBoxChange() const {
    const CloudBoxDraft* cloudDraft = session_.currentCloudDraft();
    const auto slot = firstSlotWhere(BoxSlotCount, [&](std::size_t index) {
        const PokemonPayload& pending = session_.cloud.pending[index];
        if (!cloudDraft) {
            return pending.known();
        }
        return !sameIdentity(session_.cloud.summaries[index], cloudDraft->baseline[index])
            || (pending.known() && pending.data != cloudDraft->payloads[index].data);
    });
    return slot ? std::optional(slotLocation("bank", session_.cloudBox, *slot)) : std::nullopt;
}

std::optional<std::string> StorageController::partyChange() const {
    const auto slot = firstSlotWhere(PartySlotCount,
        [&](std::size_t index) { return session_.party.slotDiffers(session_.partyBaseline, index); });
    return slot ? std::optional("party slot " + std::to_string(*slot + 1)) : std::nullopt;
}

std::optional<std::string> StorageController::trashChange() const {
    return session_.trash.empty() ? std::nullopt : std::optional<std::string>("trash box holds Pokemon awaiting deletion");
}

std::optional<std::string> StorageController::storedLocalDraftChange() const {
    for (const auto& [box, draft] : session_.localDrafts) {
        const auto baseline = session_.localBaselines.find(box);
        if (baseline == session_.localBaselines.end()) {
            return slotLocation("local box", box, 0);
        }
        const auto slot = firstSlotWhere(BoxSlotCount,
            [&](std::size_t index) { return draft.slotDiffers(baseline->second, index); });
        if (slot) {
            return slotLocation("local box", box, *slot);
        }
    }
    return std::nullopt;
}

std::optional<std::string> StorageController::storedCloudDraftChange() const {
    for (const auto& [box, draft] : session_.cloudBoxes) {
        if (box == session_.cloudKey()) {
            continue;
        }
        const auto slot = firstSlotWhere(BoxSlotCount, [&](std::size_t index) { return draft.slotChanged(index); });
        if (slot) {
            return slotLocation("bank", box, *slot);
        }
    }
    return std::nullopt;
}

void StorageController::focusFirstOccupied(const std::array<PokemonSummary, BoxSlotCount>& summaries) {
    session_.focusedSlot = firstSlotWhere(BoxSlotCount,
        [&](std::size_t index) { return summaries[index].occupied(); }).value_or(0);
}

void StorageController::loadLocalBox() {
    session_.localBoxName = session_.saveAdapter.boxName(session_.localBox);
    session_.storagePane = StoragePane::Local;
    auto baseline = session_.localBaselines.find(session_.localBox);
    if (baseline == session_.localBaselines.end()) {
        baseline = session_.localBaselines.emplace(session_.localBox,
                                                   session_.saveAdapter.readBox(session_.localBox)).first;
    }
    const auto draft = session_.localDrafts.find(session_.localBox);
    session_.local = draft != session_.localDrafts.end() ? draft->second : baseline->second;
    focusFirstOccupied(session_.local.summaries);
    Logger::instance().info("Local box loaded: " + std::to_string(session_.localBox + 1));
}

void StorageController::stepLocalBox(bool backwards) {
    const std::size_t boxCount = session_.saveAdapter.boxCount();
    persistLocalDraft();
    session_.localBox = (session_.localBox + (backwards ? boxCount - 1 : 1)) % boxCount;
    loadLocalBox();
}

void StorageController::stepCloudBox(bool backwards) {
    const std::size_t boxLimit = context_.cloudBoxLimit();
    if (session_.trashBoxActive) {
        session_.setTrashActive(false);
        session_.cloudBox = backwards ? boxLimit - 1 : 0;
        refreshCloudBox();
        return;
    }
    persistCloudDraft();
    const bool leavesRange = backwards ? session_.cloudBox == 0 : session_.cloudBox + 1 >= boxLimit;
    if (leavesRange) {
        session_.setTrashActive(true);
        loadTrashBox();
        return;
    }
    session_.cloudBox = backwards ? session_.cloudBox - 1 : session_.cloudBox + 1;
    refreshCloudBox();
}

void StorageController::loadTrashBox() {
    session_.storagePane = StoragePane::Cloud;
    session_.cloudNameFocused = false;
    focusFirstOccupied(session_.trash.summaries);
    Logger::instance().info("Trash box loaded (" + std::to_string(session_.trash.occupiedCount()) + " occupied)");
}

void StorageController::emptyTrashBox() {
    Logger::instance().info("emptyTrashBox: " + std::to_string(session_.trash.occupiedCount())
                            + " Pokemon confirmed for deletion");
    for (std::size_t slot = 0; slot < BoxSlotCount; ++slot) {
        if (session_.trash.summaries[slot].occupied()) {
            session_.confirmedDeletions.push_back(session_.trash.payloads[slot]);
        }
    }
    session_.trash = {};
}

void StorageController::persistLocalDraft() {
    const auto baseline = session_.localBaselines.find(session_.localBox);
    if (baseline == session_.localBaselines.end()) {
        return;
    }
    if (session_.local.differs(baseline->second)) {
        session_.localDrafts[session_.localBox] = session_.local;
    } else {
        session_.localDrafts.erase(session_.localBox);
    }
}

void StorageController::persistCloudDraft() {
    const auto draft = session_.cloudBoxes.find(session_.cloudKey());
    if (draft == session_.cloudBoxes.end()) {
        return;
    }
    draft->second.summaries = session_.cloud.summaries;
    draft->second.pending = session_.cloud.pending;
}

void StorageController::persistDrafts() {
    persistLocalDraft();
    persistCloudDraft();
}

void StorageController::refreshCloudBox(bool keepPreviousPreview) {
    if (!context_.signedIn()) {
        session_.cloud.clear(false);
        return;
    }
    if (const CloudBoxDraft* draft = session_.currentCloudDraft()) {
        session_.cloud.show(*draft);
        return;
    }
    const auto preview = session_.cloud.summaries;
    session_.cloud.clear(true);
    if (keepPreviousPreview) {
        session_.cloud.summaries = preview;
    }
    context_.status.clear();
    context_.loads.cloudBoxKey = session_.cloudKey();
    context_.loads.begin(LoadService::Operation::CloudBox);
}

void StorageController::resetDrafts() {
    session_.hand = Hand{};
    session_.localDrafts.clear();
    session_.localBaselines.clear();
    session_.trash = {};
    session_.confirmedDeletions.clear();
    session_.trashBoxActive = false;
}

void StorageController::discardPendingChanges() {
    const StoragePane previousPane = session_.storagePane;
    resetDrafts();
    for (auto& [key, draft] : session_.cloudBoxes) {
        draft.revert();
    }
    session_.party = session_.partyBaseline;
    loadLocalBox();
    session_.storagePane = previousPane;
    refreshCloudBox();
    context_.status = context_.text.get(TextId::ChangesDiscarded);
    Logger::instance().info("Pending storage changes discarded, cloudBox=" + std::to_string(session_.cloudBox + 1));
}

void StorageController::reloadAfterCommit(bool cloudMatchesDrafts) {
    resetDrafts();
    if (cloudMatchesDrafts) {
        for (auto& [box, draft] : session_.cloudBoxes) {
            draft.acceptPending();
        }
    } else {
        session_.cloudBoxes.clear();
        session_.cloudPrefetchCooldownUntil.clear();
    }
    session_.partyBaseline = session_.saveAdapter.readParty();
    session_.party = session_.partyBaseline;
    loadLocalBox();
    refreshCloudBox(true);
}

void StorageController::initializeFromOpenedGame(SaveLoadService::OpenGameResult& result,
                                                 BoxListResult& cloudBoxCache,
                                                 const std::vector<BoxNameEntry>& cloudBoxNames) {
    resetDrafts();
    session_.trashConfirmVisible = false;
    session_.saveSummary = std::move(result.save);
    session_.localBox = result.localBox;
    session_.cloudBox = 0;
    session_.localBoxName = std::move(result.localBoxName);
    session_.local = result.localPokemon;
    session_.localBaselines[session_.localBox] = std::move(result.localPokemon);
    session_.partyBaseline = std::move(result.localParty);
    session_.party = session_.partyBaseline;
    session_.storagePane = StoragePane::Local;
    focusFirstOccupied(session_.local.summaries);
    session_.cloudBoxNames.clear();
    for (const BoxNameEntry& entry : cloudBoxNames) {
        session_.cloudBoxNames[entry.position] = entry.name;
    }

    if (const CloudBoxDraft* existing = session_.currentCloudDraft()) {
        session_.cloud.show(*existing);
    } else if (cloudBoxCache.success) {
        session_.cloud.show(session_.cloudBoxes[session_.cloudKey()] =
            CloudBoxDraft::fromServer(cloudBoxCache.pokemon, cloudBoxCache.payloads));
    } else {
        session_.cloud.clear(true);
    }
    context_.status.clear();
    cloudBoxCache = {};
}

void StorageController::reset() {
    session_.saveAdapter.close();
    session_.resetState();
}
