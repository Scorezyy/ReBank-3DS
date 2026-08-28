#pragma once

#include "bank/BankTypes.hpp"
#include "save/SaveAdapter.hpp"
#include "save/StorageModel.hpp"

#include <3ds.h>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

// Holds all mutable state for the bank screen: the local/cloud/party
// storage the player is editing, the item currently held in hand, and the
// small bits of UI state (focus, error dialog) that the storage and cloud
// controllers need to read or update as they act on that state.
class BankSession {
public:
    int partyMemberCount() const {
        int count = 0;
        for (const PokemonSummary& member : partyWorking.summaries) {
            if (member.species != 0) {
                ++count;
            }
        }
        return count;
    }

    StorageModel storage;
    SaveAdapter saveAdapter;
    SaveSummary saveSummary;
    std::size_t localBox = 0;
    std::size_t cloudBox = 0;
    std::size_t focusedSlot = 0;
    std::string localBoxName;
    std::array<PokemonSummary, 30> cloudPreview{};
    std::array<PokemonPayload, 30> localPayloads{};
    std::array<PokemonPayload, 30> pendingUploadPayloads{};
    std::array<PokemonPayload, 30> cachedCloudPayloads{};
    std::array<bool, 30> payloadPrefetchFailed{};
    std::unordered_map<std::size_t, LocalBoxDraft> localBaselines;
    std::unordered_map<std::size_t, LocalBoxDraft> localDrafts;
    std::unordered_map<std::uint16_t, CloudBoxDraft> cloudBoxes;
    std::unordered_map<std::uint16_t, u64> cloudPrefetchCooldownUntil;
    PartyDraft partyBaseline;
    PartyDraft partyWorking;
    Hand hand;
    // Bumped every time the hand's identity changes (pick up, drop, swap,
    // return). Lets an in-flight async cloud fetch that was started for a
    // specific hand state detect it's now stale before applying its result.
    std::uint32_t handGeneration = 0;
    StoragePane storagePane = StoragePane::Local;
    bool cloudNameFocused = false;
    std::unordered_map<std::uint16_t, std::string> cloudBoxNames;
    int heldDirection = 0;
    u64 directionRepeatAt = 0;
    bool errorDialogVisible = false;
    std::string errorDialogTitle = "TRANSFER BLOCKED";
    std::string errorDialogPokemon;
    std::string errorDialogLocation;
    std::string errorDialogMessage;
};
