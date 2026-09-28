#pragma once

#include "core/BackgroundOperation.hpp"
#include "io/GameIconReader.hpp"
#include "save/adapter/SaveAdapter.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct GameSource {
    std::size_t catalogIndex = 0;
    SaveSummary save;
    bool cartridge = false;
    bool storageOnly = false;

    bool readsCartridge() const { return cartridge && !storageOnly; }
};

struct DiscoveredGame : GameSource {
    bool summaryKnown = false;
    std::shared_ptr<IconPixels> iconPixels;
};

enum class SaveOperation {
    None,
    DiscoverGames,
    RescanCartridge,
    Summary,
    OpenGame
};

enum class SavePhase {
    Idle,
    SearchingGames,
    ReadingIcons,
    ReadingSave,
    SearchingPokemon
};

class SaveLoadService : public BackgroundOperation<SaveOperation, SavePhase> {
public:
    using Operation = SaveOperation;
    using Phase = SavePhase;

    struct OpenGameResult {
        bool success = false;
        std::string message;
        SaveSummary save;
        std::size_t localBox = 0;
        std::string localBoxName;
        BoxSlots localPokemon;
        PartySlots localParty;
    };

    explicit SaveLoadService(SaveAdapter& saveAdapter)
        : BackgroundOperation("SaveLoadService"), saveAdapter_(saveAdapter) {}

    void begin(Operation operation);
    bool beginSummary(std::size_t catalogIndex, bool cartridge);
    bool beginOpen(std::size_t catalogIndex, SaveAdapter::SourcePreference preference);
    Operation poll() { return takeCompleted(); }
    void dropCartridgeGames() { discoveredGames = digitalCache_; }
    bool blocksUi() const {
        return busy() && operation() != Operation::RescanCartridge && operation() != Operation::Summary;
    }

    std::size_t catalogIndex = 0;
    SaveAdapter::SourcePreference openSourcePreference = SaveAdapter::SourcePreference::Any;

    std::vector<DiscoveredGame> discoveredGames;
    OpenGameResult openGameResult;
    SaveSummary summaryResult;
    bool summaryCartridge = false;

private:
    bool beginFor(std::size_t index, Operation operation);
    void work(Operation operation);
    std::size_t addCartridgeGame();
    void addStorageGames(std::size_t cartridgeCatalogIndex);
    void readDiscoveredIcons();
    void discoverGames();
    void rescanCartridge();
    void fetchSummary();
    void openGame();

    SaveAdapter& saveAdapter_;
    std::vector<DiscoveredGame> digitalCache_;
};
