#pragma once

#include "GameCatalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pksm {
class Sav;
}

struct PokemonSummary {
    std::uint16_t species = 0;
    std::uint16_t form = 0;
    std::uint8_t level = 0;
    bool shiny = false;
    std::string nickname;
    std::string trainerName;
    std::string gameCode;
    std::uint8_t format = 0;
};

struct SaveSummary {
    std::string trainerName;
    std::uint32_t trainerId = 0;
    std::uint32_t playTimeMinutes = 0;
    std::uint16_t pokedexCount = 0;
};

struct PokemonPayload {
    std::uint8_t format = 0;
    std::vector<std::uint8_t> data;
};

class SaveAdapter {
public:
    SaveAdapter();
    ~SaveAdapter();

    SaveAdapter(const SaveAdapter&) = delete;
    SaveAdapter& operator=(const SaveAdapter&) = delete;

    bool open(const GameDescriptor& game, std::string& error);
    void close();
    bool loaded() const;
    bool isCartridge() const;
    static std::string insertedDsGameCode();
    static bool readGameIcon(const GameDescriptor& game, bool cartridge,
                             std::array<std::uint16_t, 48 * 48>& pixels);
    SaveSummary summary() const;
    std::array<PokemonSummary, 30> readBox(std::size_t box) const;
    PokemonPayload readPokemon(std::size_t box, std::size_t slot) const;
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
    bool writeSave(std::string& error);

private:
    struct Source;

    std::unique_ptr<pksm::Sav> save_;
    std::unique_ptr<Source> source_;
    std::string gameCode_;
    std::vector<std::uint8_t> previousBuffer_;
    bool dirty_ = false;
};