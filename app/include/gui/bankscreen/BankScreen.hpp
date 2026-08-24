#pragma once

#include "network/RenameController.hpp"
#include "save/SaveAdapter.hpp"
#include "save/StorageModel.hpp"
#include "core/AsyncJob.hpp"

#include <citro2d.h>
#include <3ds.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

class App;

// The local/cloud box browser: the screen where Pokemon are actually moved.
// Owns every piece of state a box transfer touches - drafts, the held
// Pokemon, the cloud cache and the commit job that writes it all back.
class BankScreen {
public:
    explicit BankScreen(App& app) : app_(app) {}

    void update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched);
    void renderTop(float eyeOffset);
    void render();

    void pollCommit();
    void pollRenameBox();

    void onGameOpened();
    void onCloudBoxLoaded();
    void onCloudPickupCompleted();
    void onCloudSwapCompleted();

    void reset();

    bool errorDialogVisible() const { return errorDialogVisible_; }
    void dismissErrorDialog() { errorDialogVisible_ = false; }

    // Exposed so the shared background load worker can open/read the save
    // file directly; the worker runs on a thread App owns.
    SaveAdapter& saveAdapter() { return saveAdapter_; }

private:
    enum class HandSource {
        Local,
        Cloud
    };

    struct Hand {
        bool active = false;
        HandSource source = HandSource::Local;
        std::size_t sourceIndex = 0;
        std::uint16_t sourceCloudBox = 0;
        PokemonSummary summary;
        PokemonPayload payload;
        bool payloadKnown = false;
    };

    enum class StoragePane {
        Local,
        Cloud
    };

    struct LocalBoxDraft {
        std::array<PokemonSummary, 30> summaries{};
        std::array<PokemonPayload, 30> payloads{};
    };

    struct CloudBoxDraft {
        std::array<PokemonSummary, 30> summaries{};
        std::array<PokemonPayload, 30> pending{};
        std::array<PokemonSummary, 30> baseline{};
    };

    struct CommitResult {
        bool success = false;
        std::string message;
        std::string problemPokemon;
        std::string problemLocation;
        std::string problemReason;
        std::size_t uploads = 0;
        std::size_t downloads = 0;
        std::size_t deletes = 0;
    };

    int storageDirection(u32 keysDown, u32 keysHeld, circlePosition circle);
    void storagePickUp();
    void storageDrop();
    void storageReturnHand();
    bool hasPendingChanges() const;
    void loadLocalBox();
    void persistLocalDraft();
    void refreshCloudBox(bool keepPreviousPreview = false);
    void persistCloudDraft();
    void discardPendingChanges();
    void beginRenameBox(std::uint16_t position, std::string name);
    void beginCommit();
    static void commitWorker(void* argument);
    void renderStorageBottom();
    void renderErrorDialog();

    App& app_;
    StorageModel storage_;
    SaveAdapter saveAdapter_;
    SaveSummary saveSummary_;
    std::size_t localBox_ = 0;
    std::size_t cloudBox_ = 0;
    std::size_t focusedSlot_ = 0;
    std::string localBoxName_;
    std::array<PokemonSummary, 30> cloudPreview_{};
    std::array<PokemonPayload, 30> localPayloads_{};
    std::array<PokemonPayload, 30> pendingUploadPayloads_{};
    std::array<PokemonPayload, 30> cachedCloudPayloads_{};
    std::unordered_map<std::size_t, LocalBoxDraft> localBaselines_;
    std::unordered_map<std::size_t, LocalBoxDraft> localDrafts_;
    std::unordered_map<std::uint16_t, CloudBoxDraft> cloudBoxes_;
    Hand hand_;
    StoragePane storagePane_ = StoragePane::Local;
    bool cloudNameFocused_ = false;
    std::unordered_map<std::uint16_t, std::string> cloudBoxNames_;
    RenameController renameController_;
    AsyncJob commitJob_;
    CommitResult commitResult_;
    std::atomic<int> commitPhase_{0};
    std::atomic<int> commitProgress_{0};
    int heldDirection_ = 0;
    u64 directionRepeatAt_ = 0;
    bool errorDialogVisible_ = false;
    std::string errorDialogTitle_ = "TRANSFER BLOCKED";
    std::string errorDialogPokemon_;
    std::string errorDialogLocation_;
    std::string errorDialogMessage_;
};
