#include "save/adapter/SaveParser.hpp"

#include "platform/Logger.hpp"
#include "save/pokemon/PokemonTransfer.hpp"

#include <sav/Sav.hpp>
#include <sav/Sav1.hpp>
#include <sav/SavB2W2.hpp>
#include <sav/SavBW.hpp>
#include <sav/SavDP.hpp>
#include <sav/SavHGSS.hpp>
#include <sav/SavPT.hpp>

#include <exception>

namespace {
bool dsSaveSize(std::size_t size) {
    return size == 0x80000 || size == 0x8007A;
}

bool gameBoySaveSize(std::size_t size) {
    return size == 0x8000 || size == 0x8010;
}

std::unique_ptr<pksm::Sav> parseDsSave(std::string_view code, const std::shared_ptr<std::uint8_t[]>& data) {
    if (code == "diamond" || code == "pearl") {
        return std::make_unique<pksm::SavDP>(data);
    }
    if (code == "platinum") {
        return std::make_unique<pksm::SavPT>(data);
    }
    if (code == "heartgold" || code == "soulsilver") {
        return std::make_unique<pksm::SavHGSS>(data);
    }
    if (code == "black" || code == "white") {
        return std::make_unique<pksm::SavBW>(data);
    }
    if (code == "black2" || code == "white2") {
        return std::make_unique<pksm::SavB2W2>(data);
    }
    return nullptr;
}

std::unique_ptr<pksm::Sav> detect(const GameDescriptor& game, const SaveImage& image) {
    std::unique_ptr<pksm::Sav> save = dsSaveSize(image.size) ? parseDsSave(game.code, image.data) : nullptr;
    if (!save && game.format == PokemonFormat::Generation1 && gameBoySaveSize(image.size)) {
        save = std::make_unique<pksm::Sav1>(image.data, static_cast<std::uint32_t>(image.size));
    }
    if (!save) {
        save = pksm::Sav::getSave(image.data, image.size);
    }
    return save;
}
}

namespace SaveParser {
std::unique_ptr<pksm::Sav> parse(const GameDescriptor& game, const SaveImage& image, std::string& error) {
    std::unique_ptr<pksm::Sav> save;
    try {
        save = detect(game, image);
    } catch (const std::exception& exception) {
        error = "Save parser error.";
        Logger::instance().error("Save parser exception: " + std::string(exception.what()));
        return nullptr;
    }
    if (!save) {
        error = "The save format or checksum is invalid.";
        Logger::instance().error("PKSM-Core rejected save for " + std::string(game.code));
        return nullptr;
    }
    if (save->generation() != PokemonTransfer::expectedGeneration(game.format)) {
        error = "The save belongs to a different Pokemon generation.";
        Logger::instance().error("Selected game does not match detected save generation");
        return nullptr;
    }
    return save;
}
}
