#pragma once

#include "save/catalog/GameCatalog.hpp"
#include "save/pokemon/PokemonData.hpp"

#include <3ds.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pksm {
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
    bool loaded() const;
    bool isCartridge() const;
    static std::string insertedDsGameCode();
    SaveSummary summary() const;
    std::array<PokemonSummary, 30> readBox(std::size_t box) const;
    PokemonPayload readPokemon(std::size_t box, std::size_t slot) const;
    BoxRead readBoxFull(std::size_t box) const;
    std::string boxName(std::size_t box) const;
    std::size_t boxCount() const;
    std::size_t currentBox() const;
    std::uint8_t gameGeneration() const;
    bool canImportPokemon(
        std::uint8_t format,
        const std::vector<std::uint8_t>& data
    ) const;
    bool clearSlot(std::size_t box, std::size_t slot);
    bool writePokemon(
        std::size_t box,
        std::size_t slot,
        std::uint8_t format,
        const std::vector<std::uint8_t>& data
    );
    std::array<PokemonSummary, 6> readParty() const;
    PokemonPayload readPartyPokemon(std::size_t slot) const;
    std::size_t partyCount() const;
    bool clearPartySlot(std::size_t slot);
    bool writePartyPokemon(
        std::size_t slot,
        std::uint8_t format,
        const std::vector<std::uint8_t>& data
    );
    bool writeSave(std::string& error);

private:
    struct Source;

    std::shared_ptr<std::uint8_t[]> locateSave(const GameDescriptor& game, std::size_t& size, Result& result,
                                                SourcePreference preference);
    bool parseSave(const GameDescriptor& game, const std::shared_ptr<std::uint8_t[]>& data,
                    std::size_t size, std::string& error);
    bool validBox(std::size_t box) const;
    bool validSlot(std::size_t box, std::size_t slot) const;
    bool validPartySlot(std::size_t slot) const;

    std::unique_ptr<pksm::Sav> save_;
    std::unique_ptr<Source> source_;
    std::string gameCode_;
    std::vector<std::uint8_t> previousBuffer_;
    bool dirty_ = false;
};