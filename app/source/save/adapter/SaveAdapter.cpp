#include "save/adapter/SaveAdapter.hpp"
#include "core/Logger.hpp"
#include "save/catalog/VirtualConsoleTitles.hpp"
#include "save/pokemon/PokemonTransfer.hpp"
#include "io/SaveMedium.hpp"
#include "spi.hpp"

#include <3ds.h>
#include <pkx/PKX.hpp>
#include <sav/Sav.hpp>
#include <sav/SavDP.hpp>
#include <sav/SavPT.hpp>
#include <sav/SavHGSS.hpp>
#include <sav/SavBW.hpp>
#include <sav/SavB2W2.hpp>

#include <algorithm>
#include <memory>

struct SaveAdapter::Source {
    enum class Kind { None, ArchiveGameCard, ArchiveSd, DsCard, SdFile };
    Kind kind = Kind::None;
    std::uint64_t titleId = 0;
    CardType cardType = NO_CHIP;
    bool infrared = false;
    std::size_t capacity = 0;
    std::string sdPath;
};

SaveAdapter::SaveAdapter() = default;
SaveAdapter::~SaveAdapter() = default;

void SaveAdapter::close() {
    partyEdited_ = false;
    save_.reset();
    source_.reset();
    game_.reset();
    gameCode_.clear();
    previousBuffer_.clear();
    dirty_ = false;
}

bool SaveAdapter::open(const GameDescriptor& game, std::string& error, SourcePreference preference) {
    save_.reset();
    source_ = std::make_unique<Source>();
    dirty_ = false;
    partyEdited_ = false;
    game_ = game;
    gameCode_ = game.code;

    std::size_t size = 0;
    Result result = 0;
    const std::shared_ptr<std::uint8_t[]> data = locateSave(game, size, result, preference);
    if (!data) {
        error = "Save not found. Check cartridge or SD export.";
        Logger::instance().error(
            "Save open failed for " + std::string(game.code) + ": " + std::to_string(result)
        );
        return false;
    }

    if (!parseSave(game, data, size, error)) {
        return false;
    }

    previousBuffer_.assign(data.get(), data.get() + size);
    Logger::instance().info(
        "Loaded " + std::string(game.code) + " save (source=" + std::to_string(static_cast<int>(source_->kind))
        + ", bytes=" + std::to_string(size) + ", maxBoxes=" + std::to_string(save_->maxBoxes()) + ")"
    );
    return true;
}

std::shared_ptr<std::uint8_t[]> SaveAdapter::locateSave(const GameDescriptor& game, std::size_t& size, Result& result,
                                                          SourcePreference preference) {
    const bool allowCartridge = preference != SourcePreference::StorageOnly;
    const bool allowStorage = preference != SourcePreference::CartridgeOnly;

    std::shared_ptr<std::uint8_t[]> data;
    if (game.platform == GamePlatform::Nintendo3Ds) {
        data = locate3dsCartridgeSave(game, size, result, allowCartridge, allowStorage);
    } else if (game.platform == GamePlatform::VirtualConsole) {
        data = allowStorage ? locateVirtualConsoleSave(game, size, result) : nullptr;
    } else if (allowCartridge) {
        data = locateNintendoDsCartridgeSave(game, size, result);
    }
    if (data || !allowStorage) {
        return data;
    }

    std::string path;
    if (auto exported = SaveMedium::readExport(game.code, size, path)) {
        source_->kind = Source::Kind::SdFile;
        source_->sdPath = path;
        return exported;
    }
    return nullptr;
}

std::shared_ptr<std::uint8_t[]> SaveAdapter::locate3dsCartridgeSave(const GameDescriptor& game, std::size_t& size,
                                                                      Result& result, bool allowCartridge,
                                                                      bool allowStorage) {
    const auto titleId = SaveMedium::titleIdFor(game.code);
    if (!titleId) {
        return nullptr;
    }
    source_->titleId = *titleId;
    if (allowCartridge) {
        if (auto data = SaveMedium::readArchive(*titleId, MEDIATYPE_GAME_CARD, size, result)) {
            source_->kind = Source::Kind::ArchiveGameCard;
            return data;
        }
    }
    if (allowStorage) {
        if (auto data = SaveMedium::readArchive(*titleId, MEDIATYPE_SD, size, result)) {
            source_->kind = Source::Kind::ArchiveSd;
            return data;
        }
    }
    return nullptr;
}

std::shared_ptr<std::uint8_t[]> SaveAdapter::locateVirtualConsoleSave(const GameDescriptor& game, std::size_t& size,
                                                                        Result& result) {
    const auto titleId = VirtualConsoleTitles::resolveInstalledTitleId(game.code);
    if (!titleId) {
        return nullptr;
    }
    auto data = SaveMedium::readArchive(*titleId, MEDIATYPE_SD, size, result);
    if (data) {
        source_->titleId = *titleId;
        source_->kind = Source::Kind::ArchiveSd;
    }
    return data;
}

std::shared_ptr<std::uint8_t[]> SaveAdapter::locateNintendoDsCartridgeSave(const GameDescriptor& game,
                                                                             std::size_t& size, Result& result) {
    source_->infrared = game.code == "heartgold" || game.code == "soulsilver"
        || game.code == "black" || game.code == "white"
        || game.code == "black2" || game.code == "white2";
    SaveMedium::DsCardRead dsRead = SaveMedium::readDsCard(game.code, source_->infrared, result);
    if (!dsRead.data) {
        return nullptr;
    }
    source_->kind = Source::Kind::DsCard;
    source_->cardType = dsRead.cardType;
    source_->capacity = dsRead.capacity;
    size = dsRead.size;
    return std::move(dsRead.data);
}

bool SaveAdapter::parseSave(const GameDescriptor& game, const std::shared_ptr<std::uint8_t[]>& data,
                             std::size_t size, std::string& error) {
    try {
        if (size == 0x80000 || size == 0x8007A) {
            if (game.code == "diamond" || game.code == "pearl") {
                save_ = std::make_unique<pksm::SavDP>(data);
            } else if (game.code == "platinum") {
                save_ = std::make_unique<pksm::SavPT>(data);
            } else if (game.code == "heartgold" || game.code == "soulsilver") {
                save_ = std::make_unique<pksm::SavHGSS>(data);
            } else if (game.code == "black" || game.code == "white") {
                save_ = std::make_unique<pksm::SavBW>(data);
            } else if (game.code == "black2" || game.code == "white2") {
                save_ = std::make_unique<pksm::SavB2W2>(data);
            }
        }
        if (!save_) {
            save_ = pksm::Sav::getSave(data, size);
        }
    } catch (const std::exception& exception) {
        error = "Save parser error.";
        Logger::instance().error("Save parser exception: " + std::string(exception.what()));
        return false;
    }
    if (!save_) {
        error = "The save format or checksum is invalid.";
        Logger::instance().error("PKSM-Core rejected save for " + std::string(game.code));
        return false;
    }
    if (save_->generation() != PokemonTransfer::expectedGeneration(game.format)) {
        save_.reset();
        error = "The save belongs to a different Pokemon generation.";
        Logger::instance().error("Selected game does not match detected save generation");
        return false;
    }
    return true;
}

bool SaveAdapter::isCartridge() const {
    return source_ && (source_->kind == Source::Kind::ArchiveGameCard
        || source_->kind == Source::Kind::DsCard);
}

bool SaveAdapter::storageSaveExists(const GameDescriptor& game) {
    std::optional<std::uint64_t> titleId;
    if (game.platform == GamePlatform::Nintendo3Ds) {
        titleId = SaveMedium::titleIdFor(game.code);
    } else if (game.platform == GamePlatform::VirtualConsole) {
        titleId = VirtualConsoleTitles::resolveInstalledTitleId(game.code);
    }
    return (titleId && SaveMedium::archiveHasSave(*titleId, MEDIATYPE_SD)) || SaveMedium::exportExists(game.code);
}

SaveSummary SaveAdapter::summary() const {
    if (!save_) {
        return {};
    }
    return SaveSummary{
        save_->otName(),
        save_->displayTID(),
        static_cast<std::uint32_t>(save_->playedHours()) * 60 + save_->playedMinutes(),
        static_cast<std::uint16_t>(std::clamp(save_->dexCaught(), 0, 65535))
    };
}

std::string SaveAdapter::boxName(std::size_t box) const {
    return save_ && box < boxCount() ? save_->boxName(static_cast<std::uint8_t>(box)) : std::string{};
}

std::size_t SaveAdapter::boxCount() const {
    return save_ ? static_cast<std::size_t>(save_->maxBoxes()) : 0;
}

std::size_t SaveAdapter::currentBox() const {
    return save_ ? std::min<std::size_t>(save_->currentBox(), boxCount() - 1) : 0;
}

std::uint8_t SaveAdapter::gameGeneration() const {
    return save_ ? PokemonTransfer::pokemonFormat(save_->generation()) : 0;
}

bool SaveAdapter::validBox(std::size_t box) const {
    return save_ && box < boxCount();
}

std::size_t SaveAdapter::boxCapacity() const {
    if (!save_ || save_->maxBoxes() <= 0) {
        return BoxSlotCount;
    }
    return std::min<std::size_t>(BoxSlotCount, static_cast<std::size_t>(save_->maxSlot() / save_->maxBoxes()));
}

bool SaveAdapter::validSlot(std::size_t box, std::size_t slot) const {
    return validBox(box) && slot < boxCapacity();
}

bool SaveAdapter::validPartySlot(std::size_t slot) const {
    return save_ && slot < PartySlotCount;
}

std::shared_ptr<std::uint8_t[]> SaveAdapter::rereadSource(std::size_t& size) const {
    Result result = 0;
    switch (source_->kind) {
        case Source::Kind::ArchiveGameCard:
            return SaveMedium::readArchive(source_->titleId, MEDIATYPE_GAME_CARD, size, result);
        case Source::Kind::ArchiveSd:
            return SaveMedium::readArchive(source_->titleId, MEDIATYPE_SD, size, result);
        case Source::Kind::SdFile:
            return SaveMedium::readFile(source_->sdPath, size);
        case Source::Kind::DsCard: {
            SaveMedium::DsCardRead read = SaveMedium::readDsCard(gameCode_, source_->infrared, result);
            size = read.size;
            return std::move(read.data);
        }
        default:
            break;
    }
    return nullptr;
}

bool SaveAdapter::writeToSource(const std::vector<std::uint8_t>& image) {
    switch (source_->kind) {
        case Source::Kind::ArchiveGameCard:
            return SaveMedium::writeArchive(source_->titleId, MEDIATYPE_GAME_CARD, image.data(), image.size());
        case Source::Kind::ArchiveSd:
            return SaveMedium::writeArchive(source_->titleId, MEDIATYPE_SD, image.data(), image.size());
        case Source::Kind::DsCard:
            return SaveMedium::writeDsCard(source_->cardType, image.data(), image.size(),
                                           previousBuffer_.empty() ? nullptr : previousBuffer_.data(),
                                           previousBuffer_.size());
        case Source::Kind::SdFile:
            return SaveMedium::writeSdFile(source_->sdPath, image.data(), image.size());
        default:
            break;
    }
    return false;
}

void SaveAdapter::beginPartyEdit() {
    if (partyEdited_) {
        return;
    }
    partyEdited_ = true;
    auto empty = save_->emptyPkm();
    if (!empty) {
        return;
    }
    for (std::size_t slot = partyCount(); slot < PartySlotCount; ++slot) {
        save_->pkm(*empty, static_cast<std::uint8_t>(slot));
    }
}

void SaveAdapter::normalizeParty(bool compact) {
    if (!partyEdited_) {
        return;
    }
    std::vector<std::unique_ptr<pksm::PKX>> members;
    std::size_t lastOccupied = 0;
    for (std::size_t slot = 0; slot < PartySlotCount; ++slot) {
        auto member = save_->pkm(static_cast<std::uint8_t>(slot));
        if (member && static_cast<std::uint16_t>(member->species()) != 0) {
            lastOccupied = slot + 1;
            members.push_back(std::move(member));
        }
    }
    if (compact) {
        auto empty = save_->emptyPkm();
        for (std::size_t slot = 0; slot < PartySlotCount; ++slot) {
            if (slot < members.size()) {
                save_->pkm(*members[slot], static_cast<std::uint8_t>(slot));
            } else if (empty) {
                save_->pkm(*empty, static_cast<std::uint8_t>(slot));
            }
        }
        save_->partyCount(static_cast<std::uint8_t>(members.size()));
    } else {
        save_->partyCount(static_cast<std::uint8_t>(lastOccupied));
    }
    dirty_ = true;
    Logger::instance().info("normalizeParty: " + std::to_string(members.size()) + " member(s), "
                            + (compact ? std::string("compacted") : "count synced to " + std::to_string(lastOccupied)));
}

bool SaveAdapter::writeSave(std::string& error, bool finalWrite) {
    if (!save_ || !source_) {
        error = "No save loaded.";
        return false;
    }
    try {
        normalizeParty(finalWrite);
    } catch (const std::exception& exception) {
        error = "The team could not be arranged.";
        Logger::instance().error("normalizeParty exception: " + std::string(exception.what()));
        return false;
    }
    if (!dirty_) {
        return true;
    }
    try {
        save_->finishEditing();
    } catch (const std::exception& exception) {
        error = "finishEditing failed.";
        Logger::instance().error("finishEditing exception: " + std::string(exception.what()));
        return false;
    }
    const std::uint8_t* bytes = save_->rawData().get();
    const std::vector<std::uint8_t> image(bytes, bytes + save_->getLength());
    const bool written = writeToSource(image);
    save_->beginEditing();

    std::size_t diskSize = 0;
    const auto disk = rereadSource(diskSize);
    const bool diskMatches = disk && diskSize == image.size()
        && std::equal(image.begin(), image.end(), disk.get());
    if (written && (diskMatches || !disk)) {
        if (!disk) {
            Logger::instance().warning("writeSave: written, but the save could not be read back to verify it");
        }
        previousBuffer_ = image;
        dirty_ = false;
        if (finalWrite) {
            partyEdited_ = false;
        }
        Logger::instance().info("Save written and verified for " + gameCode_);
        return true;
    }
    if (disk) {
        previousBuffer_.assign(disk.get(), disk.get() + diskSize);
    }
    error = written ? "The save on disk does not match what was written." : "Failed to write save.";
    Logger::instance().error("writeSave: " + error + (disk ? " (disk state re-read)" : " (disk state unknown)"));
    return false;
}

bool SaveAdapter::discardUnsavedChanges(std::string& error) {
    if (!save_ || !game_ || previousBuffer_.empty()) {
        error = "No save loaded.";
        return false;
    }
    const std::size_t size = previousBuffer_.size();
    std::shared_ptr<std::uint8_t[]> data(new std::uint8_t[size]);
    std::copy(previousBuffer_.begin(), previousBuffer_.end(), data.get());
    save_.reset();
    if (!parseSave(*game_, data, size, error)) {
        Logger::instance().error("discardUnsavedChanges: last written save could not be parsed again");
        return false;
    }
    dirty_ = false;
    partyEdited_ = false;
    Logger::instance().info("discardUnsavedChanges: in-memory save reset to the last written state");
    return true;
}
