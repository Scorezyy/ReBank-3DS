#include "save/adapter/SaveAdapter.hpp"

#include "platform/Logger.hpp"
#include "save/adapter/SaveParser.hpp"
#include "save/pokemon/PokemonTransfer.hpp"

#include <3ds.h>
#include <pkx/PKX.hpp>
#include <sav/Sav.hpp>

#include <algorithm>
#include <memory>

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

bool SaveAdapter::open(const GameDescriptor& game, std::string& error, SaveSource::Preference preference) {
    close();
    game_ = game;
    gameCode_ = game.code;
    Logger::instance().info("Reading " + gameCode_ + " save");
    SaveImage image;
    Result result = 0;
    source_ = SaveSource::locate(game, preference, image, result);
    if (!source_) {
        error = "Save not found. Check cartridge or SD export.";
        Logger::instance().error("Save open failed for " + gameCode_ + ": " + std::to_string(result));
        return false;
    }
    save_ = SaveParser::parse(game, image, error);
    if (!save_) {
        return false;
    }
    previousBuffer_.assign(image.data.get(), image.data.get() + image.size);
    Logger::instance().info("Loaded " + gameCode_ + " save (source=" + std::to_string(source_->kindCode())
                            + ", bytes=" + std::to_string(image.size) + ", maxBoxes="
                            + std::to_string(save_->maxBoxes()) + ")");
    return true;
}

bool SaveAdapter::isCartridge() const {
    return source_ && source_->isCartridge();
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

std::optional<TrainerInfo> SaveAdapter::trainer() const {
    if (!save_) {
        return std::nullopt;
    }
    TrainerInfo trainer;
    trainer.format = saveFormat();
    trainer.name = save_->otName();
    trainer.gender = save_->gender() == pksm::Gender::Female ? 1 : 0;
    trainer.tid = save_->TID();
    trainer.sid = save_->SID();
    trainer.language = static_cast<std::uint8_t>(save_->language());
    if (trainer.format >= 6) {
        trainer.country = save_->country();
        trainer.region = save_->subRegion();
        trainer.consoleRegion = save_->consoleRegion();
    }
    if (trainer.name.empty() || trainer.language == 0) {
        return std::nullopt;
    }
    return trainer;
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
    const bool written = source_->write(image, previousBuffer_);
    save_->beginEditing();

    const std::optional<SaveImage> disk = source_->reread();
    const bool diskMatches = disk && disk->size == image.size()
        && std::equal(image.begin(), image.end(), disk->data.get());
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
        previousBuffer_.assign(disk->data.get(), disk->data.get() + disk->size);
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
    SaveImage image{std::shared_ptr<std::uint8_t[]>(new std::uint8_t[previousBuffer_.size()]), previousBuffer_.size()};
    std::copy(previousBuffer_.begin(), previousBuffer_.end(), image.data.get());
    save_ = SaveParser::parse(*game_, image, error);
    if (!save_) {
        Logger::instance().error("discardUnsavedChanges: last written save could not be parsed again");
        return false;
    }
    dirty_ = false;
    partyEdited_ = false;
    Logger::instance().info("discardUnsavedChanges: in-memory save reset to the last written state");
    return true;
}
