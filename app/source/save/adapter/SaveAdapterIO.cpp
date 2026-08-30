#include "save/adapter/SaveAdapter.hpp"
#include "core/Logger.hpp"
#include "save/pokemon/PokemonTransfer.hpp"

#include <pkx/PKX.hpp>
#include <sav/Sav.hpp>

#include <algorithm>

namespace {
std::string eggMetInfo(const pksm::PKX& pkm) {
    return "egg=" + std::string(pkm.egg() ? "1" : "0")
        + " metLocation=" + std::to_string(pkm.metLocation())
        + " metDate=" + std::to_string(pkm.metDate().year()) + "-"
        + std::to_string(pkm.metDate().month()) + "-"
        + std::to_string(pkm.metDate().day());
}
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
    if (!validPartySlot(slot) || slot >= partyCount()) {
        return {};
    }
    try {
        auto parsed = save_->pkm(static_cast<std::uint8_t>(slot));
        if (static_cast<std::uint16_t>(parsed->species()) == 0) {
            return {};
        }
        const auto raw = parsed->rawData();
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

bool SaveAdapter::canImportPokemon(
    std::uint8_t format,
    const std::vector<std::uint8_t>& data
) const {
    const std::uint8_t saveGen = gameGeneration();
    if (!save_ || data.empty() || saveGen == 0 || format == 0) {
        return false;
    }
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
