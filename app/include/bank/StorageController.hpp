#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/LoadService.hpp"
#include "bank/SlotAccess.hpp"
#include "save/SaveLoadService.hpp"

#include <optional>
#include <string>
#include <vector>

class StorageController {
public:
    StorageController(BankContext& context, BankSession& session)
        : context_(context), session_(session), slots_(session) {}

    SlotAccess& slots() { return slots_; }
    const SlotAccess& slots() const { return slots_; }

    void restorePokemon(const HeldPokemon& pokemon);

    bool hasPendingChanges() const { return pendingChange().has_value(); }
    std::optional<std::string> pendingChange() const;

    void loadLocalBox();
    void loadTrashBox();
    void stepLocalBox(bool backwards);
    void stepCloudBox(bool backwards);
    void refreshCloudBox(bool keepPreviousPreview = false);
    void persistLocalDraft();
    void persistCloudDraft();
    void persistDrafts();
    void discardPendingChanges();
    void emptyTrashBox();
    void reloadAfterCommit(bool cloudMatchesDrafts);

    void initializeFromOpenedGame(SaveLoadService::OpenGameResult& result, BoxListResult& cloudBoxCache,
                                  const std::vector<BoxNameEntry>& cloudBoxNames);
    void reset();

private:
    std::optional<std::string> openLocalBoxChange() const;
    std::optional<std::string> openCloudBoxChange() const;
    std::optional<std::string> partyChange() const;
    std::optional<std::string> trashChange() const;
    std::optional<std::string> storedLocalDraftChange() const;
    std::optional<std::string> storedCloudDraftChange() const;
    BoxSlots& localDraftForWrite(std::size_t box);
    void focusFirstOccupied(const std::array<PokemonSummary, BoxSlotCount>& summaries);
    void resetDrafts();

    BankContext& context_;
    BankSession& session_;
    SlotAccess slots_;
};
