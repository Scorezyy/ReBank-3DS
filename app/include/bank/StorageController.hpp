#pragma once

#include "bank/BankSession.hpp"
#include "bank/BankTypes.hpp"
#include "save/SaveLoadService.hpp"

#include <cstdint>
#include <string>

class App;

class StorageController {
public:
    StorageController(App& app, BankSession& session) : app_(app), session_(session) {}

    void pickUp();
    void drop();
    void returnHand();
    bool hasPendingChanges(bool verbose = false) const;

    void loadLocalBox();
    void loadTrashBox();
    void persistLocalDraft();
    void persistCloudDraft();
    void refreshCloudBox(bool keepPreviousPreview = false);
    void discardPendingChanges();
    void emptyTrashBox();

    void initializeFromOpenedGame(SaveLoadService::OpenGameResult& result);
    void reset();

private:
    void pickUpLocal();
    void pickUpParty();
    void pickUpCloud();
    void dropLocal();
    void dropParty();
    void dropCloud();
    void dropCloudTrash();
    void dropCloudBank();
    void restorePokemon(HandSource source, std::size_t sourceIndex, std::size_t sourceLocalBox,
                        std::uint16_t sourceCloudBox, bool sourceTrash, const PokemonSummary& summary,
                        const PokemonPayload& payload);
    SwapOrigin captureSwapOrigin() const;
    bool localBoxDiffers(const LocalBoxDraft& a, const LocalBoxDraft& b, std::size_t slot) const;
    bool partySlotDiffers(std::size_t slot) const;
    LocalBoxDraft& localDraftForWrite(std::size_t box);

    bool localDraftsPending(bool verbose) const;
    bool currentLocalBoxPending(bool verbose) const;
    bool cloudBoxesPending(bool verbose) const;
    bool pendingUploadsPending(bool verbose) const;
    bool trashPending(bool verbose) const;
    bool partyPending(bool verbose) const;

    App& app_;
    BankSession& session_;
};
