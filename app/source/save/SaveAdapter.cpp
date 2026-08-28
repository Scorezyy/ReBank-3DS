#include "save/SaveAdapter.hpp"
#include "core/Logger.hpp"
#include "save/PokemonTransfer.hpp"
#include "save/SaveMedium.hpp"
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
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

struct SaveAdapter::Source {
    enum class Kind { None, ArchiveGameCard, ArchiveSd, DsCard, SdFile };
    Kind kind = Kind::None;
    std::uint64_t titleId = 0;
    CardType cardType = NO_CHIP;
    bool infrared = false;
    std::size_t capacity = 0;
    std::string sdPath;
};

namespace {
// The "egg=.. metLocation=.. metDate=.." suffix shared by every box-slot
// diagnostic log line below.
std::string eggMetInfo(const pksm::PKX& pkm) {
    return "egg=" + std::string(pkm.egg() ? "1" : "0")
        + " metLocation=" + std::to_string(pkm.metLocation())
        + " metDate=" + std::to_string(pkm.metDate().year()) + "-"
        + std::to_string(pkm.metDate().month()) + "-"
        + std::to_string(pkm.metDate().day());
}
}

SaveAdapter::SaveAdapter() = default;
SaveAdapter::~SaveAdapter() = default;

void SaveAdapter::close() {
    save_.reset();
    source_.reset();
    gameCode_.clear();
    previousBuffer_.clear();
    dirty_ = false;
}

bool SaveAdapter::open(const GameDescriptor& game, std::string& error) {
    save_.reset();
    source_ = std::make_unique<Source>();
    dirty_ = false;
    gameCode_ = game.code;

    std::size_t size = 0;
    Result result = 0;
    const std::shared_ptr<std::uint8_t[]> data = locateSave(game, size, result);
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

// Tries, in order: a 3DS title's save archive (cartridge, then SD), a DS
// cartridge over SPI, and finally a save exported to the SD card. Populates
// `source_` with whichever one actually produced data, so writeSave() later
// knows where to persist changes back to.
std::shared_ptr<std::uint8_t[]> SaveAdapter::locateSave(const GameDescriptor& game, std::size_t& size, Result& result) {
    if (game.platform == GamePlatform::Nintendo3Ds) {
        const auto mapping = std::find_if(SaveMedium::TitleMappings.begin(), SaveMedium::TitleMappings.end(),
            [&](const auto& item) { return item.code == game.code; });
        if (mapping != SaveMedium::TitleMappings.end()) {
            source_->titleId = mapping->titleId;
            if (auto data = SaveMedium::readArchive(mapping->titleId, MEDIATYPE_GAME_CARD, size, result)) {
                source_->kind = Source::Kind::ArchiveGameCard;
                return data;
            }
            if (auto data = SaveMedium::readArchive(mapping->titleId, MEDIATYPE_SD, size, result)) {
                source_->kind = Source::Kind::ArchiveSd;
                return data;
            }
        }
    } else {
        source_->infrared = game.code == "heartgold" || game.code == "soulsilver"
            || game.code == "black" || game.code == "white"
            || game.code == "black2" || game.code == "white2";
        SaveMedium::DsCardRead dsRead = SaveMedium::readDsCard(game.code, source_->infrared, result);
        if (dsRead.data) {
            source_->kind = Source::Kind::DsCard;
            source_->cardType = dsRead.cardType;
            source_->capacity = dsRead.capacity;
            size = dsRead.size;
            return std::move(dsRead.data);
        }
    }

    std::string path;
    if (auto data = SaveMedium::readExport(game.code, size, path)) {
        source_->kind = Source::Kind::SdFile;
        source_->sdPath = path;
        return data;
    }
    return nullptr;
}

// Picks the right pksm::Sav subclass and confirms it matches the
// generation the selected game expects.
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

std::array<PokemonSummary, 30> SaveAdapter::readBox(std::size_t box) const {
    std::array<PokemonSummary, 30> pokemon{};
    if (!validBox(box)) {
        return pokemon;
    }
    try {
        for (std::size_t slot = 0; slot < pokemon.size(); ++slot) {
            auto parsed = save_->pkm(static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot));
            const auto species = static_cast<std::uint16_t>(parsed->species());
            if (species == 0) {
                continue;
            }
            pokemon[slot] = PokemonSummary{
                species,
                parsed->alternativeForm(),
                parsed->level(),
                parsed->shiny(),
                parsed->heldItem(),
                parsed->nickname(),
                parsed->otName(),
                gameCode_,
                PokemonTransfer::pokemonFormat(save_->generation()),
                parsed->type1(),
                parsed->type2(),
                parsed->version(),
                parsed->language(),
                {parsed->move(0), parsed->move(1), parsed->move(2), parsed->move(3)},
                parsed->ability(),
                parsed->nature(),
                parsed->gender()
            };
        }
    } catch (const std::exception& exception) {
        Logger::instance().error("Box parse exception: " + std::string(exception.what()));
    }
    return pokemon;
}

BoxRead SaveAdapter::readBoxFull(std::size_t box) const {
    BoxRead result;
    if (!validBox(box)) {
        return result;
    }
    try {
        for (std::size_t slot = 0; slot < result.summaries.size(); ++slot) {
            auto parsed = save_->pkm(static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot));
            const auto species = static_cast<std::uint16_t>(parsed->species());
            if (species == 0) {
                continue;
            }
            result.summaries[slot] = PokemonSummary{
                species,
                parsed->alternativeForm(),
                parsed->level(),
                parsed->shiny(),
                parsed->heldItem(),
                parsed->nickname(),
                parsed->otName(),
                gameCode_,
                PokemonTransfer::pokemonFormat(save_->generation()),
                parsed->type1(),
                parsed->type2(),
                parsed->version(),
                parsed->language(),
                {parsed->move(0), parsed->move(1), parsed->move(2), parsed->move(3)},
                parsed->ability(),
                parsed->nature(),
                parsed->gender()
            };
            const auto raw = parsed->rawData();
            result.payloads[slot] = {
                PokemonTransfer::pokemonFormat(save_->generation()),
                std::vector<std::uint8_t>(raw.begin(), raw.end())
            };
        }
    } catch (const std::exception& exception) {
        Logger::instance().error("Box parse exception: " + std::string(exception.what()));
    }
    return result;
}

PokemonPayload SaveAdapter::readPokemon(std::size_t box, std::size_t slot) const {
    if (!validSlot(box, slot)) {
        return {};
    }
    try {
        auto parsed = save_->pkm(static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot));
        if (static_cast<std::uint16_t>(parsed->species()) == 0) {
            return {};
        }
        const auto raw = parsed->rawData();
        Logger::instance().info("readPokemon: box " + std::to_string(box + 1) + " slot "
                                + std::to_string(slot + 1) + " species "
                                + std::to_string(static_cast<std::uint16_t>(parsed->species()))
                                + " format " + std::to_string(PokemonTransfer::pokemonFormat(save_->generation()))
                                + " checksum 0x" + std::to_string(parsed->checksum())
                                + " " + eggMetInfo(*parsed)
                                + " bytes " + std::to_string(raw.size()));
        return {
            PokemonTransfer::pokemonFormat(save_->generation()),
            std::vector<std::uint8_t>(raw.begin(), raw.end())
        };
    } catch (const std::exception& exception) {
        Logger::instance().error("Pokemon payload exception: " + std::string(exception.what()));
        return {};
    }
}

std::array<PokemonSummary, 6> SaveAdapter::readParty() const {
    std::array<PokemonSummary, 6> party{};
    if (!save_) {
        return party;
    }
    try {
        const std::size_t count = std::min<std::size_t>(save_->partyCount(), party.size());
        for (std::size_t slot = 0; slot < count; ++slot) {
            auto parsed = save_->pkm(static_cast<std::uint8_t>(slot));
            const auto species = static_cast<std::uint16_t>(parsed->species());
            if (species == 0) {
                continue;
            }
            party[slot] = PokemonSummary{
                species,
                parsed->alternativeForm(),
                parsed->level(),
                parsed->shiny(),
                parsed->heldItem(),
                parsed->nickname(),
                parsed->otName(),
                gameCode_,
                PokemonTransfer::pokemonFormat(save_->generation()),
                parsed->type1(),
                parsed->type2(),
                parsed->version(),
                parsed->language(),
                {parsed->move(0), parsed->move(1), parsed->move(2), parsed->move(3)},
                parsed->ability(),
                parsed->nature(),
                parsed->gender()
            };
        }
    } catch (const std::exception& exception) {
        Logger::instance().error("Party parse exception: " + std::string(exception.what()));
    }
    return party;
}

PokemonPayload SaveAdapter::readPartyPokemon(std::size_t slot) const {
    // validPartySlot() only checks slot < 6, not the save's actual party
    // size - save_->pkm() on a slot beyond the real party count reads
    // uninitialized/out-of-bounds data in the underlying save library and
    // crashes hard (not a C++ exception, so the try/catch below can't help).
    // readParty() already bounds by partyCount() for the preview; this needs
    // the same bound for the raw payload read.
    if (!validPartySlot(slot) || slot >= partyCount()) {
        return {};
    }
    try {
        auto parsed = save_->pkm(static_cast<std::uint8_t>(slot));
        if (static_cast<std::uint16_t>(parsed->species()) == 0) {
            return {};
        }
        const auto raw = parsed->rawData();
        Logger::instance().info("readPartyPokemon: slot " + std::to_string(slot + 1) + " species "
                                + std::to_string(static_cast<std::uint16_t>(parsed->species()))
                                + " format " + std::to_string(PokemonTransfer::pokemonFormat(save_->generation()))
                                + " bytes " + std::to_string(raw.size()));
        return {
            PokemonTransfer::pokemonFormat(save_->generation()),
            std::vector<std::uint8_t>(raw.begin(), raw.end())
        };
    } catch (const std::exception& exception) {
        Logger::instance().error("Party payload exception: " + std::string(exception.what()));
        return {};
    }
}

std::size_t SaveAdapter::partyCount() const {
    return save_ ? std::min<std::size_t>(save_->partyCount(), 6) : 0;
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

bool SaveAdapter::canImportPokemon(
    std::uint8_t format,
    const std::vector<std::uint8_t>& data
) const {
    const std::uint8_t saveGen = gameGeneration();
    if (!save_ || data.empty() || saveGen == 0 || format == 0) {
        return false;
    }
    // Only equal or upward transfers are allowed. A Pokemon can never move to an
    // older generation, so anything newer than the save is rejected outright.
    return format <= saveGen;
}

bool SaveAdapter::clearSlot(std::size_t box, std::size_t slot) {
    if (!validSlot(box, slot)) {
        return false;
    }
    try {
        auto empty = save_->emptyPkm();
        if (!empty) {
            return false;
        }
        save_->pkm(*empty, static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot), false);
        auto written = save_->pkm(static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot));
        Logger::instance().info("clearSlot: box " + std::to_string(box + 1) + " slot "
                                + std::to_string(slot + 1) + " readback species "
                                + std::to_string(written ? static_cast<std::uint16_t>(written->species()) : 0)
                                + " egg=" + (written && written->egg() ? "1" : "0"));
        dirty_ = true;
        return true;
    } catch (const std::exception& exception) {
        Logger::instance().error("clearSlot exception: " + std::string(exception.what()));
        return false;
    }
}

bool SaveAdapter::writePokemon(
    std::size_t box,
    std::size_t slot,
    std::uint8_t format,
    const std::vector<std::uint8_t>& data
) {
    if (!validSlot(box, slot) || data.empty()) {
        return false;
    }
    const std::uint8_t saveGen = gameGeneration();
    try {
        auto buffer = std::vector<std::uint8_t>(data);
        const pksm::Generation gen = PokemonTransfer::generationFromFormat(format);
        if (gen == pksm::Generation::UNUSED) {
            return false;
        }
        auto pkx = pksm::PKX::getPKM(gen, buffer.data(), buffer.size(), false);
        if (!pkx || static_cast<std::uint16_t>(pkx->species()) == 0) {
            Logger::instance().warning("writePokemon: getPKM returned null/empty species");
            return false;
        }
        Logger::instance().info("writePokemon: box " + std::to_string(box + 1) + " slot "
                                + std::to_string(slot + 1) + " parsed gen " + std::to_string(format)
                                + " species " + std::to_string(static_cast<std::uint16_t>(pkx->species()))
                                + " " + eggMetInfo(*pkx)
                                + " bytes=" + std::to_string(data.size()));
        std::unique_ptr<pksm::PKX> converted;
        if (format != saveGen) {
            Logger::instance().info("writePokemon: converting Gen " + std::to_string(format)
                                    + " to Gen " + std::to_string(saveGen));
            converted = PokemonTransfer::convertForSave(*pkx, format, saveGen, *save_);
            if (!converted) {
                Logger::instance().warning("Unsupported or failed conversion from Gen "
                                           + std::to_string(format) + " to Gen "
                                           + std::to_string(saveGen));
                return false;
            }
            Logger::instance().info("writePokemon: converted to gen " + std::to_string(saveGen)
                                    + " species " + std::to_string(static_cast<std::uint16_t>(converted->species())));
        }
        pksm::PKX& pkmRef = converted ? *converted : *pkx;
        const auto expectedGeneration = pkmRef.generation();
        const auto expectedSpecies = pkmRef.species();
        const std::uint16_t storedChecksum = pkmRef.checksum();
        pkmRef.refreshChecksum();
        const std::uint16_t recomputedChecksum = pkmRef.checksum();
        if (storedChecksum != recomputedChecksum) {
            Logger::instance().warning("writePokemon: checksum mismatch before write (stored 0x"
                                       + std::to_string(storedChecksum) + " recomputed 0x"
                                       + std::to_string(recomputedChecksum) + ") format "
                                       + std::to_string(format) + " species "
                                       + std::to_string(static_cast<std::uint16_t>(expectedSpecies)));
            pkmRef.checksum(storedChecksum);
        }
        save_->pkm(pkmRef, static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot), false);
        auto written = save_->pkm(static_cast<std::uint8_t>(box), static_cast<std::uint8_t>(slot));
        if (!written || written->generation() != expectedGeneration
            || written->species() != expectedSpecies) {
            Logger::instance().error("writePokemon readback failed for box " + std::to_string(box + 1)
                                     + " slot " + std::to_string(slot + 1));
            return false;
        }
        if (written->egg() && !pkx->egg()) {
            Logger::instance().error("writePokemon: write turned a non-egg into an egg for box "
                                     + std::to_string(box + 1) + " slot " + std::to_string(slot + 1)
                                     + "; refusing to persist this write");
            return false;
        }
        Logger::instance().info("writePokemon readback verified species "
                                + std::to_string(static_cast<std::uint16_t>(written->species()))
                                + " " + eggMetInfo(*written));
        dirty_ = true;
        return true;
    } catch (const std::exception& exception) {
        Logger::instance().error("writePokemon exception: " + std::string(exception.what()));
        return false;
    }
}

bool SaveAdapter::clearPartySlot(std::size_t slot) {
    if (!validPartySlot(slot)) {
        return false;
    }
    try {
        auto empty = save_->emptyPkm();
        if (!empty) {
            return false;
        }
        save_->pkm(*empty, static_cast<std::uint8_t>(slot));
        auto written = save_->pkm(static_cast<std::uint8_t>(slot));
        Logger::instance().info("clearPartySlot: slot " + std::to_string(slot + 1)
                                + " readback species "
                                + std::to_string(written ? static_cast<std::uint16_t>(written->species()) : 0)
                                + " egg=" + (written && written->egg() ? "1" : "0"));
        dirty_ = true;
        return true;
    } catch (const std::exception& exception) {
        Logger::instance().error("clearPartySlot exception: " + std::string(exception.what()));
        return false;
    }
}

bool SaveAdapter::writePartyPokemon(
    std::size_t slot,
    std::uint8_t format,
    const std::vector<std::uint8_t>& data
) {
    if (!validPartySlot(slot) || data.empty()) {
        return false;
    }
    const std::uint8_t saveGen = gameGeneration();
    try {
        auto buffer = std::vector<std::uint8_t>(data);
        const pksm::Generation gen = PokemonTransfer::generationFromFormat(format);
        if (gen == pksm::Generation::UNUSED) {
            return false;
        }
        auto pkx = pksm::PKX::getPKM(gen, buffer.data(), buffer.size(), false);
        if (!pkx || static_cast<std::uint16_t>(pkx->species()) == 0) {
            Logger::instance().warning("writePartyPokemon: getPKM returned null/empty species");
            return false;
        }
        Logger::instance().info("writePartyPokemon: slot " + std::to_string(slot + 1)
                                + " parsed gen " + std::to_string(format)
                                + " species " + std::to_string(static_cast<std::uint16_t>(pkx->species()))
                                + " " + eggMetInfo(*pkx)
                                + " bytes=" + std::to_string(data.size()));
        std::unique_ptr<pksm::PKX> converted;
        if (format != saveGen) {
            Logger::instance().info("writePartyPokemon: converting Gen " + std::to_string(format)
                                    + " to Gen " + std::to_string(saveGen));
            converted = PokemonTransfer::convertForSave(*pkx, format, saveGen, *save_);
            if (!converted) {
                Logger::instance().warning("Unsupported or failed party conversion from Gen "
                                           + std::to_string(format) + " to Gen "
                                           + std::to_string(saveGen));
                return false;
            }
        }
        pksm::PKX& pkmRef = converted ? *converted : *pkx;
        const auto expectedGeneration = pkmRef.generation();
        const auto expectedSpecies = pkmRef.species();
        const std::uint16_t storedChecksum = pkmRef.checksum();
        pkmRef.refreshChecksum();
        const std::uint16_t recomputedChecksum = pkmRef.checksum();
        if (storedChecksum != recomputedChecksum) {
            Logger::instance().warning("writePartyPokemon: checksum mismatch before write (stored 0x"
                                       + std::to_string(storedChecksum) + " recomputed 0x"
                                       + std::to_string(recomputedChecksum) + ")");
            pkmRef.checksum(storedChecksum);
        }
        save_->pkm(pkmRef, static_cast<std::uint8_t>(slot));
        auto written = save_->pkm(static_cast<std::uint8_t>(slot));
        if (!written || written->generation() != expectedGeneration
            || written->species() != expectedSpecies) {
            Logger::instance().error("writePartyPokemon readback failed for slot " + std::to_string(slot + 1));
            return false;
        }
        if (written->egg() && !pkx->egg()) {
            Logger::instance().error("writePartyPokemon: write turned a non-egg into an egg for slot "
                                     + std::to_string(slot + 1) + "; refusing to persist this write");
            return false;
        }
        Logger::instance().info("writePartyPokemon readback verified species "
                                + std::to_string(static_cast<std::uint16_t>(written->species()))
                                + " " + eggMetInfo(*written));
        dirty_ = true;
        return true;
    } catch (const std::exception& exception) {
        Logger::instance().error("writePartyPokemon exception: " + std::string(exception.what()));
        return false;
    }
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
