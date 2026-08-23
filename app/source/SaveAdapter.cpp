#include "SaveAdapter.hpp"
#include "FsGuard.hpp"
#include "Logger.hpp"
#include "spi.hpp"

#include <3ds.h>
#include <pkx/PKX.hpp>
#include <pkx/PK4.hpp>
#include <pkx/PK5.hpp>
#include <pkx/PK6.hpp>
#include <pkx/PK7.hpp>
#include <sav/Sav.hpp>
#include <sav/SavDP.hpp>
#include <sav/SavPT.hpp>
#include <sav/SavHGSS.hpp>
#include <sav/SavBW.hpp>
#include <sav/SavB2W2.hpp>
#include <utils/DateTime.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>
#include <sys/stat.h>

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
constexpr std::size_t MaximumSaveSize = 0x200000;

struct TitleMapping {
    std::string_view code;
    std::uint64_t titleId;
};

constexpr std::array TitleMappings{
    TitleMapping{"x", 0x0004000000055D00ULL},
    TitleMapping{"y", 0x0004000000055E00ULL},
    TitleMapping{"omega-ruby", 0x000400000011C400ULL},
    TitleMapping{"alpha-sapphire", 0x000400000011C500ULL},
    TitleMapping{"sun", 0x0004000000164800ULL},
    TitleMapping{"moon", 0x0004000000175E00ULL},
    TitleMapping{"ultra-sun", 0x00040000001B5000ULL},
    TitleMapping{"ultra-moon", 0x00040000001B5100ULL}
};

std::shared_ptr<std::uint8_t[]> readFile(const std::string& path, std::size_t& size) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return nullptr;
    }
    std::fseek(file, 0, SEEK_END);
    const long fileSize = std::ftell(file);
    std::rewind(file);
    if (fileSize <= 0 || static_cast<std::size_t>(fileSize) > MaximumSaveSize) {
        std::fclose(file);
        return nullptr;
    }
    size = static_cast<std::size_t>(fileSize);
    auto data = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[size]());
    const bool success = std::fread(data.get(), 1, size, file) == size;
    std::fclose(file);
    return success ? data : nullptr;
}

std::shared_ptr<std::uint8_t[]> readArchive(
    std::uint64_t titleId,
    FS_MediaType mediaType,
    std::size_t& size,
    Result& result
) {
    const std::uint32_t pathData[3] = {
        static_cast<std::uint32_t>(mediaType),
        static_cast<std::uint32_t>(titleId),
        static_cast<std::uint32_t>(titleId >> 32)
    };
    FS_Archive archive{};
    result = FSUSER_OpenArchive(
        &archive,
        ARCHIVE_USER_SAVEDATA,
        FS_Path{PATH_BINARY, sizeof(pathData), pathData}
    );
    if (R_FAILED(result)) {
        return nullptr;
    }

    Handle file = 0;
    result = FSUSER_OpenFile(
        &file,
        archive,
        fsMakePath(PATH_ASCII, "/main"),
        FS_OPEN_READ,
        0
    );
    if (R_FAILED(result)) {
        FSUSER_CloseArchive(archive);
        return nullptr;
    }

    std::uint64_t fileSize = 0;
    std::uint32_t bytesRead = 0;
    result = FSFILE_GetSize(file, &fileSize);
    if (R_SUCCEEDED(result) && fileSize > 0 && fileSize <= MaximumSaveSize) {
        size = static_cast<std::size_t>(fileSize);
        auto data = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[size]());
        result = FSFILE_Read(file, &bytesRead, 0, data.get(), static_cast<std::uint32_t>(size));
        FSFILE_Close(file);
        FSUSER_CloseArchive(archive);
        if (R_SUCCEEDED(result) && bytesRead == size) {
            return data;
        }
    }

    FSFILE_Close(file);
    FSUSER_CloseArchive(archive);
    return nullptr;
}

std::shared_ptr<std::uint8_t[]> readExport(
    std::string_view code,
    std::size_t& size,
    std::string& path
) {
    const std::array paths{
        "sdmc:/3ds/ReBank/saves/" + std::string(code) + "/main",
        "sdmc:/3ds/ReBank/saves/" + std::string(code) + ".sav"
    };
    for (const auto& candidate : paths) {
        if (auto data = readFile(candidate, size)) {
            path = candidate;
            return data;
        }
    }
    return nullptr;
}

pksm::Generation expectedGeneration(PokemonFormat format) {
    switch (format) {
        case PokemonFormat::Generation4:
            return pksm::Generation::FOUR;
        case PokemonFormat::Generation5:
            return pksm::Generation::FIVE;
        case PokemonFormat::Generation6:
            return pksm::Generation::SIX;
        case PokemonFormat::Generation7:
            return pksm::Generation::SEVEN;
    }
    return pksm::Generation::UNUSED;
}

std::uint8_t pokemonFormat(pksm::Generation generation) {
    if (generation == pksm::Generation::FOUR) {
        return 4;
    }
    if (generation == pksm::Generation::FIVE) {
        return 5;
    }
    if (generation == pksm::Generation::SIX) {
        return 6;
    }
    if (generation == pksm::Generation::SEVEN) {
        return 7;
    }
    return 0;
}

bool isGen4Hm(pksm::Move move) {
    return move == pksm::Move::Cut || move == pksm::Move::Fly
        || move == pksm::Move::Surf || move == pksm::Move::Strength
        || move == pksm::Move::Whirlpool || move == pksm::Move::RockSmash
        || move == pksm::Move::Waterfall || move == pksm::Move::RockClimb;
}

std::uint16_t gen4TransferLocation(const pksm::PKX& pokemon) {
    if (!pokemon.fatefulEncounter()) {
        return 30001;
    }
    switch (pokemon.species()) {
        case pksm::Species::Raikou:
        case pksm::Species::Entei:
        case pksm::Species::Suicune:
            return 30012;
        case pksm::Species::Celebi:
            return 30010;
        default:
            return 30001;
    }
}

void copyTransferMoves(const pksm::PKX& source, pksm::PKX& target, bool removeGen4Hms) {
    std::uint8_t destination = 0;
    for (std::uint8_t index = 0; index < 4; ++index) {
        const pksm::Move move = source.move(index);
        if (move == pksm::Move::None || (removeGen4Hms && isGen4Hm(move))) {
            continue;
        }
        target.move(destination, move);
        target.PPUp(destination, std::min<std::uint8_t>(source.PPUp(index), 3));
        target.PP(destination, source.PP(index));
        ++destination;
    }
    while (destination < 4) {
        target.move(destination, pksm::Move::None);
        target.PPUp(destination, 0);
        target.PP(destination, 0);
        ++destination;
    }
}

std::unique_ptr<pksm::PKX> convertG4ToG5Stable(const pksm::PKX& source) {
    if (source.generation() != pksm::Generation::FOUR) {
        return nullptr;
    }
    std::array<std::uint8_t, pksm::PK5::BOX_LENGTH> raw{};
    const auto sourceRaw = source.rawData();
    std::copy_n(sourceRaw.begin(), std::min(raw.size(), sourceRaw.size()), raw.begin());
    auto target = pksm::PKX::getPKM<pksm::Generation::FIVE>(
        raw.data(), raw.size());
    if (!target) {
        return nullptr;
    }

    const std::uint32_t originalPid = source.PID();
    auto& pk5 = static_cast<pksm::PK5&>(*target);
    std::fill(target->rawData().begin() + 0x42, target->rawData().begin() + 0x48, 0);
    std::fill(target->rawData().begin() + 0x86, target->rawData().begin() + 0x88, 0);
    target->heldItem(0);
    target->otFriendship(70);
    target->nature(source.nature());
    target->metDate(Date::today());
    target->metLocation(gen4TransferLocation(source));
    target->metLevel(source.level());
    target->nickname(source.nickname());
    target->otName(source.otName());
    target->ball(source.ball());
    pk5.hiddenAbility(false);
    pk5.nPokemon(false);
    copyTransferMoves(source, *target, true);
    target->PID(originalPid);
    target->refreshChecksum();
    if (target->PID() != originalPid) {
        Logger::instance().error("Gen 4 to Gen 5 conversion changed PID");
        return nullptr;
    }
    Logger::instance().info("Gen 4 to Gen 5: PID preserved "
                            + std::to_string(originalPid));
    return target;
}

std::uint8_t gen6AbilitySlot(
    const pksm::PKX& pokemon,
    pksm::Ability ability,
    bool hiddenAbility
) {
    if (hiddenAbility && ability == pokemon.abilities(2)) {
        return 4;
    }
    if (ability == pokemon.abilities(0)) {
        return 1;
    }
    if (ability == pokemon.abilities(1)) {
        return 2;
    }
    if (ability == pokemon.abilities(2)) {
        return 4;
    }
    return hiddenAbility ? 4 : 1;
}

std::unique_ptr<pksm::PKX> convertLegacyToG6Stable(const pksm::PKX& source, pksm::Sav& save) {
    auto target = pksm::PKX::getPKM<pksm::Generation::SIX>(nullptr, pksm::PK6::BOX_LENGTH);
    Logger::instance().info("Legacy to Gen 6: target allocated");
    const auto species = static_cast<std::uint16_t>(source.species());
    const bool fromGen4 = source.generation() == pksm::Generation::FOUR;
    const bool fromGen5 = source.generation() == pksm::Generation::FIVE;
    if (!target || (!fromGen4 && !fromGen5) || species == 0
        || species > (fromGen4 ? 493 : 649)) {
        return nullptr;
    }

    target->encryptionConstant(source.PID());
    target->species(source.species());
    target->heldItem(0);
    target->TID(source.TID());
    target->SID(source.SID());
    target->experience(source.experience());
    target->PID(source.PID());
    target->ability(source.ability());
    target->nature(source.nature());
    target->fatefulEncounter(source.fatefulEncounter());
    target->gender(source.gender());
    target->alternativeForm(source.alternativeForm());
    const auto sourceAbility = source.ability();
    const bool hiddenAbility = fromGen5 && source.abilityNumber() == 4;
    const std::uint8_t abilitySlot = gen6AbilitySlot(*target, sourceAbility, hiddenAbility);
    if (sourceAbility != target->abilities(0)
        && sourceAbility != target->abilities(1)
        && sourceAbility != target->abilities(2)) {
        Logger::instance().warning("Legacy to Gen 6: ability ID not present in Gen 6 personal data");
    }
    target->abilityNumber(abilitySlot);
    target->egg(source.egg());
    target->nicknamed(true);
    target->nickname(source.nickname());
    target->otName(source.otName());
    target->otGender(source.otGender());
    target->version(source.version());
    target->ball(source.ball());
    target->metLevel(fromGen4 ? source.level() : source.metLevel());
    target->metLocation(fromGen4 ? 30001 : source.metLocation());
    target->metDate(fromGen4 ? Date::today() : source.metDate());
    target->eggLocation(fromGen4 ? 0 : source.eggLocation());
    target->pkrsDays(source.pkrsDays());
    target->pkrsStrain(source.pkrsStrain());
    auto& pk6 = static_cast<pksm::PK6&>(*target);
    pk6.encounterType(fromGen4
        ? static_cast<const pksm::PK4&>(source).encounterType()
        : static_cast<const pksm::PK5&>(source).encounterType());

    const auto languageValue = static_cast<std::uint8_t>(source.language());
    target->language(languageValue >= 1 && languageValue <= 8 && languageValue != 6
        ? source.language()
        : pksm::Language::ENG);
    Logger::instance().info("Legacy to Gen 6: identity copied");

    for (std::uint8_t index = 0; index < 6; ++index) {
        const auto stat = pksm::Stat(index);
        target->ev(stat, std::min<std::uint16_t>(source.ev(stat), 252));
        target->iv(stat, source.iv(stat));
        target->contest(index, source.contest(index));
    }
    copyTransferMoves(source, *target, fromGen4);
    Logger::instance().info("Legacy to Gen 6: stats and moves copied");

    target->currentHandler(pksm::PKXHandler::NonOT);
    pk6.htName(save.otName());
    pk6.htGender(save.gender());
    pk6.htFriendship(70);
    pk6.region(save.subRegion());
    pk6.country(save.country());
    pk6.consoleRegion(save.consoleRegion());
    target->refreshChecksum();
    Logger::instance().info(
        "Legacy to Gen 6: ready abilitySlot=" + std::to_string(target->abilityNumber())
        + " metLevel=" + std::to_string(target->metLevel())
        + " metLocation=" + std::to_string(target->metLocation())
        + " encounterType=" + std::to_string(pk6.encounterType()));
    return target;
}

// Pokemon may only move to an equal or newer generation. Downward conversion is
// intentionally unsupported: it is the source of lost data and bad eggs. A mon
// that reached Gen 6 stays Gen 6 and can no longer be placed into a Gen 4/5 save.
std::unique_ptr<pksm::PKX> convertForSave(
    const pksm::PKX& source,
    std::uint8_t sourceFormat,
    std::uint8_t targetGeneration,
    pksm::Sav& save
) {
    if (sourceFormat >= targetGeneration) {
        return nullptr;
    }
    if (sourceFormat == 4 && targetGeneration == 5) {
        return convertG4ToG5Stable(source);
    }
    if ((sourceFormat == 4 || sourceFormat == 5) && targetGeneration == 6) {
        return convertLegacyToG6Stable(source, save);
    }
    if ((sourceFormat == 4 || sourceFormat == 5) && targetGeneration == 7) {
        auto gen6 = convertLegacyToG6Stable(source, save);
        return gen6 ? gen6->convertToG7(save) : nullptr;
    }
    if (sourceFormat == 6 && targetGeneration == 7) {
        return source.convertToG7(save);
    }
    return nullptr;
}

std::string dsGameCodeFromHeader() {
    FS_CardType cardType = CARD_CTR;
    if (R_FAILED(FSUSER_GetCardType(&cardType)) || cardType != CARD_TWL) {
        return {};
    }
    std::array<std::uint8_t, 0x3B4> header{};
    if (R_FAILED(FSUSER_GetLegacyRomHeader(MEDIATYPE_GAME_CARD, 0, header.data()))) {
        return {};
    }
    const std::string prefix(reinterpret_cast<const char*>(header.data() + 0x0C), 3);
    static constexpr std::array mappings{
        std::pair{"ADA", "diamond"}, std::pair{"APA", "pearl"},
        std::pair{"CPU", "platinum"}, std::pair{"IPK", "heartgold"},
        std::pair{"IPG", "soulsilver"}, std::pair{"IRB", "black"},
        std::pair{"IRA", "white"}, std::pair{"IRE", "black2"},
        std::pair{"IRD", "white2"}
    };
    const auto match = std::find_if(mappings.begin(), mappings.end(),
        [&](const auto& mapping) { return prefix == mapping.first; });
    return match == mappings.end() ? std::string{} : std::string(match->second);
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
    std::shared_ptr<std::uint8_t[]> data;

    if (game.platform == GamePlatform::Nintendo3Ds) {
        const auto mapping = std::find_if(TitleMappings.begin(), TitleMappings.end(),
            [&](const auto& item) { return item.code == game.code; });
        if (mapping != TitleMappings.end()) {
            source_->titleId = mapping->titleId;
            data = readArchive(mapping->titleId, MEDIATYPE_GAME_CARD, size, result);
            if (data) {
                source_->kind = Source::Kind::ArchiveGameCard;
            } else {
                data = readArchive(mapping->titleId, MEDIATYPE_SD, size, result);
                if (data) {
                    source_->kind = Source::Kind::ArchiveSd;
                }
            }
        }
    } else {
        Result dsResult = 0;
        source_->infrared = game.code == "heartgold" || game.code == "soulsilver"
            || game.code == "black" || game.code == "white"
            || game.code == "black2" || game.code == "white2";
        const std::string insertedCode = dsGameCodeFromHeader();
        dsResult = insertedCode == game.code ? pxiDevInit() : -1;
        if (insertedCode == game.code && R_SUCCEEDED(dsResult)) {
            CardType cardType = NO_CHIP;
            dsResult = SPIGetCardType(&cardType, source_->infrared ? 1 : 0);
            if (R_SUCCEEDED(dsResult) && cardType != NO_CHIP) {
                const std::uint32_t capacity = SPIGetCapacity(cardType);
                if (capacity == 0x80000) {
                    auto buffer = std::shared_ptr<std::uint8_t[]>(new std::uint8_t[capacity]());
                    bool ok = true;
                    constexpr std::uint32_t SectorSize = 0x10000;
                    for (std::uint32_t offset = 0; offset < capacity && ok; offset += SectorSize) {
                        if (R_FAILED(SPIReadSaveData(cardType, offset, buffer.get() + offset, SectorSize))) {
                            ok = false;
                        }
                    }
                    if (ok) {
                        data = buffer;
                        size = capacity;
                        source_->kind = Source::Kind::DsCard;
                        source_->cardType = cardType;
                        source_->capacity = capacity;
                    }
                }
            }
            pxiDevExit();
        }
        result = dsResult;
    }

    if (!data) {
        std::string path;
        data = readExport(game.code, size, path);
        if (data) {
            source_->kind = Source::Kind::SdFile;
            source_->sdPath = path;
        }
    }
    if (!data) {
        error = "Save not found. Check cartridge or SD export.";
        Logger::instance().error(
            "Save open failed for " + std::string(game.code) + ": " + std::to_string(result)
        );
        return false;
    }

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
    if (save_->generation() != expectedGeneration(game.format)) {
        save_.reset();
        error = "The save belongs to a different Pokemon generation.";
        Logger::instance().error("Selected game does not match detected save generation");
        return false;
    }

    previousBuffer_.assign(data.get(), data.get() + size);

    Logger::instance().info(
        "Loaded " + std::string(game.code) + " save (source=" + std::to_string(static_cast<int>(source_->kind)) + ")"
    );
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
    return dsGameCodeFromHeader();
}

bool SaveAdapter::readGameIcon(const GameDescriptor& game, bool cartridge,
                               std::array<std::uint16_t, 48 * 48>& pixels) {
    const FsGuard guard;
    pixels.fill(0);
    if (game.platform == GamePlatform::NintendoDs && cartridge) {
        const std::unique_ptr<std::uint8_t[]> banner(
            new (std::nothrow) std::uint8_t[0x23C0]());
        if (!banner
            || R_FAILED(FSUSER_GetLegacyBannerData(MEDIATYPE_GAME_CARD, 0, banner.get()))) {
            return false;
        }
        const auto* bitmap = banner.get() + 0x20;
        const auto* palette = reinterpret_cast<const std::uint16_t*>(banner.get() + 0x220);
        for (std::size_t y = 0; y < 48; ++y) {
            for (std::size_t x = 0; x < 48; ++x) {
                const std::size_t sourceX = x * 32 / 48;
                const std::size_t sourceY = y * 32 / 48;
                const std::size_t tileOffset = ((sourceY / 8) * 4 + sourceX / 8) * 32
                    + (sourceY % 8) * 4 + (sourceX % 8) / 2;
                const std::uint8_t packed = bitmap[tileOffset];
                const std::uint8_t index = (sourceX & 1) ? packed >> 4 : packed & 0x0F;
                const std::uint16_t bgr555 = palette[index];
                const std::uint16_t red = bgr555 & 0x1F;
                const std::uint16_t green = (bgr555 >> 5) & 0x1F;
                const std::uint16_t blue = (bgr555 >> 10) & 0x1F;
                pixels[y * 48 + x] = static_cast<std::uint16_t>(
                    (red << 11) | ((green << 1) << 5) | blue);
            }
        }
        return true;
    }

    const auto mapping = std::find_if(TitleMappings.begin(), TitleMappings.end(),
        [&](const auto& item) { return item.code == game.code; });
    if (mapping == TitleMappings.end()) {
        return false;
    }
    const FS_MediaType mediaType = cartridge ? MEDIATYPE_GAME_CARD : MEDIATYPE_SD;
    const std::uint32_t archivePathData[4] = {
        static_cast<std::uint32_t>(mapping->titleId),
        static_cast<std::uint32_t>(mapping->titleId >> 32),
        static_cast<std::uint32_t>(mediaType),
        0
    };
    static constexpr std::uint32_t IconFilePathData[5] = {
        0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000
    };
    Handle file = 0;
    Result result = FSUSER_OpenFileDirectly(&file, ARCHIVE_SAVEDATA_AND_CONTENT,
        FS_Path{PATH_BINARY, sizeof(archivePathData), archivePathData},
        FS_Path{PATH_BINARY, sizeof(IconFilePathData), IconFilePathData},
        FS_OPEN_READ, 0);
    if (R_FAILED(result)) {
        return false;
    }
    const std::unique_ptr<std::uint8_t[]> smdh(
        new (std::nothrow) std::uint8_t[0x36C0]());
    if (!smdh) {
        FSFILE_Close(file);
        return false;
    }
    std::uint32_t bytesRead = 0;
    result = FSFILE_Read(file, &bytesRead, 0, smdh.get(), 0x36C0);
    FSFILE_Close(file);
    if (R_FAILED(result) || bytesRead != 0x36C0
        || std::memcmp(smdh.get(), "SMDH", 4) != 0) {
        return false;
    }
    const auto* icon = reinterpret_cast<const std::uint16_t*>(smdh.get() + 0x24C0);
    for (std::size_t tileY = 0; tileY < 6; ++tileY) {
        for (std::size_t tileX = 0; tileX < 6; ++tileX) {
            for (std::size_t pixel = 0; pixel < 64; ++pixel) {
                const std::size_t source = (tileY * 6 + tileX) * 64 + pixel;
                const std::size_t x = tileX * 8 + ((pixel & 1) | ((pixel >> 1) & 2) | ((pixel >> 2) & 4));
                const std::size_t y = tileY * 8 + (((pixel >> 1) & 1) | ((pixel >> 2) & 2) | ((pixel >> 3) & 4));
                pixels[y * 48 + x] = icon[source];
            }
        }
    }
    return true;
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
    if (!save_ || box >= boxCount()) {
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
                pokemonFormat(save_->generation())
            };
        }
    } catch (const std::exception& exception) {
        Logger::instance().error("Box parse exception: " + std::string(exception.what()));
    }
    return pokemon;
}

PokemonPayload SaveAdapter::readPokemon(std::size_t box, std::size_t slot) const {
    if (!save_ || box >= boxCount() || slot >= 30) {
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
                                + " format " + std::to_string(pokemonFormat(save_->generation()))
                                + " checksum 0x" + std::to_string(parsed->checksum())
                                + " egg=" + (parsed->egg() ? "1" : "0")
                                + " metLocation=" + std::to_string(parsed->metLocation())
                                + " metDate=" + std::to_string(parsed->metDate().year()) + "-"
                                + std::to_string(parsed->metDate().month()) + "-"
                                + std::to_string(parsed->metDate().day())
                                + " bytes " + std::to_string(raw.size()));
        return {
            pokemonFormat(save_->generation()),
            std::vector<std::uint8_t>(raw.begin(), raw.end())
        };
    } catch (const std::exception& exception) {
        Logger::instance().error("Pokemon payload exception: " + std::string(exception.what()));
        return {};
    }
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
    return save_ ? pokemonFormat(save_->generation()) : 0;
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
    if (!save_ || box >= boxCount() || slot >= 30) {
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
    if (!save_ || box >= boxCount() || slot >= 30 || data.empty()) {
        return false;
    }
    const std::uint8_t saveGen = gameGeneration();
    try {
        auto buffer = std::vector<std::uint8_t>(data);
        pksm::Generation gen = pksm::Generation::UNUSED;
        switch (format) {
            case 4: gen = pksm::Generation::FOUR; break;
            case 5: gen = pksm::Generation::FIVE; break;
            case 6: gen = pksm::Generation::SIX; break;
            case 7: gen = pksm::Generation::SEVEN; break;
            default: return false;
        }
        auto pkx = pksm::PKX::getPKM(gen, buffer.data(), buffer.size(), false);
        if (!pkx || static_cast<std::uint16_t>(pkx->species()) == 0) {
            Logger::instance().warning("writePokemon: getPKM returned null/empty species");
            return false;
        }
        Logger::instance().info("writePokemon: box " + std::to_string(box + 1) + " slot "
                                + std::to_string(slot + 1) + " parsed gen " + std::to_string(format)
                                + " species " + std::to_string(static_cast<std::uint16_t>(pkx->species()))
                                + " egg=" + (pkx->egg() ? "1" : "0")
                                + " metLocation=" + std::to_string(pkx->metLocation())
                                + " metDate=" + std::to_string(pkx->metDate().year()) + "-"
                                + std::to_string(pkx->metDate().month()) + "-"
                                + std::to_string(pkx->metDate().day())
                                + " bytes=" + std::to_string(data.size()));
        std::unique_ptr<pksm::PKX> converted;
        if (format != saveGen) {
            Logger::instance().info("writePokemon: converting Gen " + std::to_string(format)
                                    + " to Gen " + std::to_string(saveGen));
            converted = convertForSave(*pkx, format, saveGen, *save_);
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
                                + " egg=" + (written->egg() ? "1" : "0")
                                + " metLocation=" + std::to_string(written->metLocation())
                                + " metDate=" + std::to_string(written->metDate().year()) + "-"
                                + std::to_string(written->metDate().month()) + "-"
                                + std::to_string(written->metDate().day()));
        dirty_ = true;
        return true;
    } catch (const std::exception& exception) {
        Logger::instance().error("writePokemon exception: " + std::string(exception.what()));
        return false;
    }
}

namespace {
bool writeArchive(std::uint64_t titleId, FS_MediaType mediaType, const std::uint8_t* data, std::size_t size) {
    const std::uint32_t pathData[3] = {
        static_cast<std::uint32_t>(mediaType),
        static_cast<std::uint32_t>(titleId),
        static_cast<std::uint32_t>(titleId >> 32)
    };
    FS_Archive archive{};
    Result result = FSUSER_OpenArchive(
        &archive, ARCHIVE_USER_SAVEDATA,
        FS_Path{PATH_BINARY, sizeof(pathData), pathData}
    );
    if (R_FAILED(result)) {
        return false;
    }
    Handle file = 0;
    result = FSUSER_OpenFile(
        &file, archive, fsMakePath(PATH_ASCII, "/main"),
        FS_OPEN_WRITE, 0
    );
    if (R_FAILED(result)) {
        FSUSER_CloseArchive(archive);
        return false;
    }
    std::uint32_t bytesWritten = 0;
    result = FSFILE_Write(file, &bytesWritten, 0, data, static_cast<std::uint32_t>(size), FS_WRITE_FLUSH);
    FSFILE_Close(file);
    if (R_SUCCEEDED(result)) {
        result = FSUSER_ControlArchive(archive, ARCHIVE_ACTION_COMMIT_SAVE_DATA, nullptr, 0, nullptr, 0);
    }
    FSUSER_CloseArchive(archive);
    return R_SUCCEEDED(result) && bytesWritten == size;
}

bool writeSdFile(const std::string& path, const std::uint8_t* data, std::size_t size) {
    ::mkdir("sdmc:/3ds", 0777);
    ::mkdir("sdmc:/3ds/ReBank", 0777);
    ::mkdir("sdmc:/3ds/ReBank/saves", 0777);
    const std::string backup = path + ".bak";
    FILE* backupExists = std::fopen(backup.c_str(), "rb");
    if (!backupExists) {
        FILE* src = std::fopen(path.c_str(), "rb");
        if (src) {
            FILE* dst = std::fopen(backup.c_str(), "wb");
            if (dst) {
                std::array<std::uint8_t, 4096> buffer{};
                std::size_t read = 0;
                while ((read = std::fread(buffer.data(), 1, buffer.size(), src)) > 0) {
                    std::fwrite(buffer.data(), 1, read, dst);
                }
                std::fclose(dst);
            }
            std::fclose(src);
        }
    } else {
        std::fclose(backupExists);
    }
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        return false;
    }
    const std::size_t written = std::fwrite(data, 1, size, file);
    std::fclose(file);
    return written == size;
}

bool writeDsCard(CardType cardType, const std::uint8_t* data, std::size_t size, bool infrared,
                 const std::uint8_t* previous, std::size_t previousSize) {
    if (R_FAILED(pxiDevInit())) {
        return false;
    }
    constexpr std::uint32_t SectorSize = 0x10000;
    bool ok = true;
    std::size_t writtenSectors = 0;
    for (std::uint32_t offset = 0; offset < size && ok; offset += SectorSize) {
        const std::uint32_t chunk = std::min<std::uint32_t>(SectorSize, static_cast<std::uint32_t>(size - offset));
        const bool unchanged = previous != nullptr
            && previousSize >= static_cast<std::size_t>(offset) + chunk
            && std::memcmp(previous + offset, data + offset, chunk) == 0;
        if (unchanged) {
            continue;
        }
        if (R_FAILED(SPIEraseSector(cardType, offset))
            || R_FAILED(SPIWriteSaveData(cardType, offset, const_cast<std::uint8_t*>(data + offset), chunk))) {
            ok = false;
        } else {
            ++writtenSectors;
        }
    }
    pxiDevExit();
    (void)infrared;
    Logger::instance().info("DS card save wrote " + std::to_string(writtenSectors) + " sectors");
    return ok;
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
            ok = writeArchive(source_->titleId, MEDIATYPE_GAME_CARD, bytes, size);
            break;
        case Source::Kind::ArchiveSd:
            ok = writeArchive(source_->titleId, MEDIATYPE_SD, bytes, size);
            break;
        case Source::Kind::DsCard:
            ok = writeDsCard(source_->cardType, bytes, size, source_->infrared,
                             previousBuffer_.empty() ? nullptr : previousBuffer_.data(),
                             previousBuffer_.size());
            break;
        case Source::Kind::SdFile:
            ok = writeSdFile(source_->sdPath, bytes, size);
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
        auto reread = readArchive(source_->titleId, mediaType, verifySize, verifyResult);
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