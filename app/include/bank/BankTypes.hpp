#pragma once

#include "save/pokemon/PokemonData.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

enum class StoragePane {
    Local,
    Cloud,
    Party
};

struct StorageAddress {
    StoragePane pane = StoragePane::Local;
    bool trash = false;

    constexpr bool isCloudBank() const { return pane == StoragePane::Cloud && !trash; }
    constexpr bool isTrashCan() const { return pane == StoragePane::Cloud && trash; }
    constexpr bool isInSave() const { return pane != StoragePane::Cloud; }

    friend constexpr bool operator==(StorageAddress, StorageAddress) = default;
};

struct HeldPokemon {
    StorageAddress source;
    std::size_t sourceIndex = 0;
    std::size_t sourceLocalBox = 0;
    std::uint16_t sourceCloudBox = 0;
    PokemonSummary summary;
    PokemonPayload payload;
};

struct Hand : HeldPokemon {
    bool active = false;
    bool payloadKnown = false;
    std::vector<HeldPokemon> swapHistory;

    void take(HeldPokemon pokemon) {
        static_cast<HeldPokemon&>(*this) = std::move(pokemon);
        swapHistory.clear();
        active = true;
        payloadKnown = payload.known();
    }

    void swapWith(HeldPokemon occupant) {
        swapHistory.push_back(*this);
        static_cast<HeldPokemon&>(*this) = std::move(occupant);
        payloadKnown = payload.known();
    }
};

struct CloudBoxDraft {
    std::array<PokemonSummary, BoxSlotCount> summaries{};
    std::array<PokemonPayload, BoxSlotCount> pending{};
    std::array<PokemonSummary, BoxSlotCount> baseline{};
    std::array<PokemonPayload, BoxSlotCount> payloads{};

    static CloudBoxDraft fromServer(const std::array<PokemonSummary, BoxSlotCount>& pokemon,
                                    const std::array<PokemonPayload, BoxSlotCount>& serverPayloads) {
        CloudBoxDraft draft;
        draft.baseline = pokemon;
        draft.summaries = pokemon;
        draft.payloads = serverPayloads;
        return draft;
    }

    bool slotChanged(std::size_t slot) const {
        return !sameIdentity(summaries[slot], baseline[slot])
            || (pending[slot].known() && pending[slot].data != payloads[slot].data);
    }

    void acceptPending() {
        for (std::size_t slot = 0; slot < BoxSlotCount; ++slot) {
            if (pending[slot].known()) {
                payloads[slot] = std::move(pending[slot]);
            } else if (slotChanged(slot)) {
                payloads[slot] = {};
            }
        }
        baseline = summaries;
        pending = {};
    }

    void revert() {
        summaries = baseline;
        pending = {};
    }
};

struct CloudView {
    std::array<PokemonSummary, BoxSlotCount> summaries{};
    std::array<PokemonPayload, BoxSlotCount> pending{};
    std::array<PokemonPayload, BoxSlotCount> cached{};
    std::array<bool, BoxSlotCount> prefetchFailed{};
    bool awaitingLoad = false;

    void show(const CloudBoxDraft& draft) {
        summaries = draft.summaries;
        pending = draft.pending;
        cached = draft.payloads;
        prefetchFailed = {};
        awaitingLoad = false;
    }

    void clear(bool awaitLoad) {
        summaries.fill({});
        pending = {};
        cached = {};
        prefetchFailed = {};
        awaitingLoad = awaitLoad;
    }
};
