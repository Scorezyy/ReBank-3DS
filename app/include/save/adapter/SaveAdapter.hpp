#pragma once

#include "save/catalog/GameCatalog.hpp"
#include "save/pokemon/PokemonData.hpp"
#include "save/storage/SaveSource.hpp"

#include <3ds.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pksm {
class PKX;
class Sav;
}

struct SaveSummary {
    std::string trainerName;
    std::uint32_t trainerId = 0;
    std::uint32_t playTimeMinutes = 0;
    std::uint16_t pokedexCount = 0;
};

class SaveAdapter {
public:
    SaveAdapter();
    ~SaveAdapter();

    SaveAdapter(const SaveAdapter&) = delete;
    SaveAdapter& operator=(const SaveAdapter&) = delete;

    bool open(const GameDescriptor& game, std::string& error, SaveSource::Preference preference = SaveSource::Preference::Any);
    void close();
    bool isCartridge() const;
    SaveSummary summary() const;
    std::optional<TrainerInfo> trainer() const;
    BoxSlots readBox(std::size_t box) const;
    bool slotOccupied(std::size_t box, std::size_t slot) const;
    std::string boxName(std::size_t box) const;
    std::size_t boxCount() const;
    std::size_t boxCapacity() const;
    std::size_t currentBox() const;
    std::uint8_t gameGeneration() const;
    bool canImportPokemon(const PokemonPayload& payload) const;
    bool clearSlot(std::size_t box, std::size_t slot);
    bool writePokemon(std::size_t box, std::size_t slot, const PokemonPayload& payload);
    PartySlots readParty() const;
    std::size_t partyCount() const;
    bool clearPartySlot(std::size_t slot);
    bool writePartyPokemon(std::size_t slot, const PokemonPayload& payload);
    bool writeSave(std::string& error, bool finalWrite = true);
    bool discardUnsavedChanges(std::string& error);

private:
    bool validBox(std::size_t box) const;
    bool validSlot(std::size_t box, std::size_t slot) const;
    bool validPartySlot(std::size_t slot) const;
    std::uint8_t saveFormat() const;
    void beginPartyEdit();
    void normalizeParty(bool compact);
    template <typename Edit>
    bool edit(const char* action, Edit&& change);
    bool storeEmpty(const std::function<void(const pksm::PKX&)>& store);

    std::unique_ptr<pksm::Sav> save_;
    std::optional<SaveSource> source_;
    std::optional<GameDescriptor> game_;
    std::string gameCode_;
    std::vector<std::uint8_t> previousBuffer_;
    bool dirty_ = false;
    bool partyEdited_ = false;
};