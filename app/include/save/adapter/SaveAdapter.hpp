#pragma once

#include "save/catalog/GameCatalog.hpp"
#include "save/pokemon/PokemonData.hpp"

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
    enum class SourcePreference { Any, CartridgeOnly, StorageOnly };

    SaveAdapter();
    ~SaveAdapter();

    SaveAdapter(const SaveAdapter&) = delete;
    SaveAdapter& operator=(const SaveAdapter&) = delete;

    bool open(const GameDescriptor& game, std::string& error, SourcePreference preference = SourcePreference::Any);
    void close();
    bool isCartridge() const;
    static bool storageSaveExists(const GameDescriptor& game);
    SaveSummary summary() const;
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
    struct Source;

    std::shared_ptr<std::uint8_t[]> locateSave(const GameDescriptor& game, std::size_t& size, Result& result,
                                                SourcePreference preference);
    std::shared_ptr<std::uint8_t[]> locate3dsCartridgeSave(const GameDescriptor& game, std::size_t& size,
                                                             Result& result, bool allowCartridge, bool allowStorage);
    std::shared_ptr<std::uint8_t[]> locateVirtualConsoleSave(const GameDescriptor& game, std::size_t& size,
                                                                Result& result);
    std::shared_ptr<std::uint8_t[]> locateNintendoDsCartridgeSave(const GameDescriptor& game, std::size_t& size,
                                                                     Result& result);
    bool parseSave(const GameDescriptor& game, const std::shared_ptr<std::uint8_t[]>& data,
                    std::size_t size, std::string& error);
    bool validBox(std::size_t box) const;
    bool validSlot(std::size_t box, std::size_t slot) const;
    bool validPartySlot(std::size_t slot) const;
    std::uint8_t saveFormat() const;
    std::shared_ptr<std::uint8_t[]> rereadSource(std::size_t& size) const;
    bool writeToSource(const std::vector<std::uint8_t>& image);
    void beginPartyEdit();
    void normalizeParty(bool compact);
    template <typename Edit>
    bool edit(const char* action, Edit&& change);
    bool storeEmpty(const std::function<void(const pksm::PKX&)>& store);

    std::unique_ptr<pksm::Sav> save_;
    std::unique_ptr<Source> source_;
    std::optional<GameDescriptor> game_;
    std::string gameCode_;
    std::vector<std::uint8_t> previousBuffer_;
    bool dirty_ = false;
    bool partyEdited_ = false;
};