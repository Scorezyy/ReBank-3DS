#pragma once

#include "bank/BankTypes.hpp"
#include "save/adapter/SaveAdapter.hpp"
#include "selection/SelectionState.hpp"

#include <3ds.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct BankState {
    SaveSummary saveSummary;
    std::size_t localBox = 0;
    std::size_t cloudBox = 0;
    std::size_t focusedSlot = 0;
    StoragePane storagePane = StoragePane::Local;
    std::string localBoxName;

    BoxSlots local;
    std::unordered_map<std::size_t, BoxSlots> localBaselines;
    std::unordered_map<std::size_t, BoxSlots> localDrafts;
    PartySlots partyBaseline;
    PartySlots party;

    CloudView cloud;
    std::unordered_map<std::uint16_t, CloudBoxDraft> cloudBoxes;
    std::unordered_map<std::uint16_t, u64> cloudPrefetchCooldownUntil;
    std::unordered_map<std::uint16_t, std::string> cloudBoxNames;
    bool cloudNameFocused = false;

    BoxSlots trash;
    std::vector<PokemonPayload> confirmedDeletions;
    bool trashBoxActive = false;
    bool trashConfirmVisible = false;
    u64 trashTransitionStart = 0;

    Hand hand;
    std::uint32_t handGeneration = 0;
    SelectionState selection;
};

class BankSession : public BankState {
public:
    explicit BankSession(SaveAdapter& adapter) : saveAdapter(adapter) {}

    void resetState() {
        const std::uint32_t generation = handGeneration;
        static_cast<BankState&>(*this) = BankState{};
        handGeneration = generation + 1;
    }

    std::uint16_t cloudKey() const { return static_cast<std::uint16_t>(cloudBox); }
    std::uint16_t cloudPosition() const { return static_cast<std::uint16_t>(cloudBox + 1); }

    const CloudBoxDraft* currentCloudDraft() const {
        const auto it = cloudBoxes.find(cloudKey());
        return it == cloudBoxes.end() ? nullptr : &it->second;
    }

    void setTrashActive(bool active) {
        trashBoxActive = active;
        trashTransitionStart = svcGetSystemTick();
    }

    GridGeometry grid(StoragePane pane) const { return GridGeometry::forPane(pane, saveAdapter.boxCapacity()); }

    StorageAddress focusedAddress() const {
        return {storagePane, storagePane == StoragePane::Cloud && trashBoxActive};
    }

    SaveAdapter& saveAdapter;
};
