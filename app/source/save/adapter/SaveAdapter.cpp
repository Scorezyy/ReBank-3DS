#include "save/adapter/SaveAdapter.hpp"
#include "core/Logger.hpp"
#include "save/catalog/VirtualConsoleTitles.hpp"
#include "save/pokemon/PokemonTransfer.hpp"
#include "io/SaveMedium.hpp"
#include "spi.hpp"

#include <3ds.h>
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
    save_.reset();
    source_.reset();
    gameCode_.clear();
    previousBuffer_.clear();
    dirty_ = false;
}

bool SaveAdapter::open(const GameDescriptor& game, std::string& error, SourcePreference preference) {
    save_.reset();
    source_ = std::make_unique<Source>();
    dirty_ = false;
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
    const auto mapping = std::find_if(SaveMedium::TitleMappings.begin(), SaveMedium::TitleMappings.end(),
        [&](const auto& item) { return item.code == game.code; });
    if (mapping == SaveMedium::TitleMappings.end()) {
        return nullptr;
    }
    source_->titleId = mapping->titleId;
    if (allowCartridge) {
        if (auto data = SaveMedium::readArchive(mapping->titleId, MEDIATYPE_GAME_CARD, size, result)) {
            source_->kind = Source::Kind::ArchiveGameCard;
            return data;
        }
    }
    if (allowStorage) {
        if (auto data = SaveMedium::readArchive(mapping->titleId, MEDIATYPE_SD, size, result)) {
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

bool SaveAdapter::loaded() const {
    return save_ != nullptr;
}

bool SaveAdapter::isCartridge() const {
    return source_ && (source_->kind == Source::Kind::ArchiveGameCard
        || source_->kind == Source::Kind::DsCard);
}

std::string SaveAdapter::insertedDsGameCode() {
    return SaveMedium::dsGameCodeFromHeader();
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

bool SaveAdapter::validSlot(std::size_t box, std::size_t slot) const {
    return validBox(box) && slot < 30;
}

bool SaveAdapter::validPartySlot(std::size_t slot) const {
    return save_ && slot < 6;
}

bool SaveAdapter::writeSave(std::string& error) {
    if (!save_ || !source_) {
        error = "No save loaded.";
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

    const auto& raw = save_->rawData();
    const std::size_t size = save_->getLength();
    const std::uint8_t* bytes = raw.get();

    bool ok = false;
    switch (source_->kind) {
        case Source::Kind::ArchiveGameCard:
            ok = SaveMedium::writeArchive(source_->titleId, MEDIATYPE_GAME_CARD, bytes, size);
            break;
        case Source::Kind::ArchiveSd:
            ok = SaveMedium::writeArchive(source_->titleId, MEDIATYPE_SD, bytes, size);
            break;
        case Source::Kind::DsCard:
            ok = SaveMedium::writeDsCard(source_->cardType, bytes, size, source_->infrared,
                             previousBuffer_.empty() ? nullptr : previousBuffer_.data(),
                             previousBuffer_.size());
            break;
        case Source::Kind::SdFile:
            ok = SaveMedium::writeSdFile(source_->sdPath, bytes, size);
            break;
        default:
            break;
    }

    save_->beginEditing();
    if (!ok) {
        error = "Failed to write save.";
        Logger::instance().error("writeSave: destination write failed");
        return false;
    }
    previousBuffer_.assign(bytes, bytes + size);
    dirty_ = false;
    Logger::instance().info("Save written for " + gameCode_);

    if (source_->kind == Source::Kind::ArchiveGameCard || source_->kind == Source::Kind::ArchiveSd) {
        std::size_t verifySize = 0;
        Result verifyResult = 0;
        const FS_MediaType mediaType = source_->kind == Source::Kind::ArchiveGameCard
            ? MEDIATYPE_GAME_CARD : MEDIATYPE_SD;
        auto reread = SaveMedium::readArchive(source_->titleId, mediaType, verifySize, verifyResult);
        if (!reread || verifySize != size) {
            Logger::instance().error("writeSave verify: re-read failed or size mismatch (got "
                                     + std::to_string(verifySize) + " expected "
                                     + std::to_string(size) + ")");
        } else {
            std::size_t firstMismatch = SIZE_MAX;
            std::size_t mismatchCount = 0;
            for (std::size_t i = 0; i < size; ++i) {
                if (reread[i] != bytes[i]) {
                    if (firstMismatch == SIZE_MAX) {
                        firstMismatch = i;
                    }
                    ++mismatchCount;
                }
            }
            if (mismatchCount == 0) {
                Logger::instance().info("writeSave verify: on-disk bytes match in-memory buffer");
            } else {
                Logger::instance().error("writeSave verify: " + std::to_string(mismatchCount)
                                         + " byte(s) differ, first at offset 0x"
                                         + std::to_string(firstMismatch));
            }
        }
    }
    return true;
}
