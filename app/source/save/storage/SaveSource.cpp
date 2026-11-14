#include "save/storage/SaveSource.hpp"

#include "save/catalog/VirtualConsoleTitles.hpp"
#include "save/storage/Cartridge.hpp"
#include "save/storage/RomLibrary.hpp"
#include "save/storage/SaveArchive.hpp"
#include "save/storage/SaveFiles.hpp"
#include "save/storage/TitleIds.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace {
constexpr std::array<std::string_view, 6> InfraredGames{"heartgold", "soulsilver", "black", "white", "black2", "white2"};

bool hasInfrared(std::string_view code) {
    return std::find(InfraredGames.begin(), InfraredGames.end(), code) != InfraredGames.end();
}

std::optional<std::uint64_t> storageTitleId(const GameDescriptor& game) {
    if (game.platform == GamePlatform::Nintendo3Ds) {
        return TitleIds::titleIdFor(game.code);
    }
    if (game.platform == GamePlatform::VirtualConsole) {
        return VirtualConsoleTitles::resolveInstalledTitleId(game.code);
    }
    return std::nullopt;
}

bool readArchive(std::uint64_t titleId, FS_MediaType media, SaveImage& image, Result& result) {
    image.data = SaveArchive::read(titleId, media, image.size, result);
    return image.data != nullptr;
}
}

std::optional<SaveSource> SaveSource::locate(const GameDescriptor& game, Preference preference, SaveImage& image,
                                             Result& result) {
    const bool allowCartridge = preference != Preference::StorageOnly;
    const bool allowStorage = preference != Preference::CartridgeOnly;
    std::optional<SaveSource> source;
    if (game.platform == GamePlatform::Nintendo3Ds) {
        source = locate3ds(game, allowCartridge, allowStorage, image, result);
    } else if (game.platform == GamePlatform::VirtualConsole && allowStorage) {
        source = locateVirtualConsole(game, image, result);
    } else if (game.platform == GamePlatform::NintendoDs && allowCartridge) {
        source = locateDsCard(game, image, result);
    }
    if (source || !allowStorage) {
        return source;
    }
    return locateSdFile(game, image);
}

bool SaveSource::existsInStorage(const GameDescriptor& game) {
    const std::optional<std::uint64_t> titleId = storageTitleId(game);
    return (titleId && SaveArchive::hasSave(*titleId, MEDIATYPE_SD)) || SaveFiles::exportExists(game.code)
        || (RomLibrary::supports(game.platform) && RomLibrary::find(game.code));
}

std::optional<SaveSource> SaveSource::locate3ds(const GameDescriptor& game, bool allowCartridge, bool allowStorage,
                                                SaveImage& image, Result& result) {
    const std::optional<std::uint64_t> titleId = TitleIds::titleIdFor(game.code);
    if (!titleId) {
        return std::nullopt;
    }
    SaveSource source;
    source.gameCode_ = game.code;
    source.titleId_ = *titleId;
    if (allowCartridge && readArchive(*titleId, MEDIATYPE_GAME_CARD, image, result)) {
        source.kind_ = Kind::ArchiveGameCard;
        return source;
    }
    if (allowStorage && readArchive(*titleId, MEDIATYPE_SD, image, result)) {
        source.kind_ = Kind::ArchiveSd;
        return source;
    }
    return std::nullopt;
}

std::optional<SaveSource> SaveSource::locateVirtualConsole(const GameDescriptor& game, SaveImage& image,
                                                           Result& result) {
    const std::optional<std::uint64_t> titleId = VirtualConsoleTitles::resolveInstalledTitleId(game.code);
    if (!titleId || !readArchive(*titleId, MEDIATYPE_SD, image, result)) {
        return std::nullopt;
    }
    SaveSource source;
    source.kind_ = Kind::ArchiveSd;
    source.gameCode_ = game.code;
    source.titleId_ = *titleId;
    return source;
}

std::optional<SaveSource> SaveSource::locateDsCard(const GameDescriptor& game, SaveImage& image, Result& result) {
    const bool infrared = hasInfrared(game.code);
    Cartridge::DsCardRead card = Cartridge::readDsSave(game.code, infrared, result);
    if (!card.data) {
        return std::nullopt;
    }
    image = {std::move(card.data), card.size};
    SaveSource source;
    source.kind_ = Kind::DsCard;
    source.gameCode_ = game.code;
    source.cardType_ = card.cardType;
    source.infrared_ = infrared;
    return source;
}

std::optional<SaveSource> SaveSource::locateSdFile(const GameDescriptor& game, SaveImage& image) {
    std::string path;
    image.data = SaveFiles::readExport(game.code, image.size, path);
    const std::optional<RomSave> rom = !image.data && RomLibrary::supports(game.platform)
        ? RomLibrary::find(game.code)
        : std::nullopt;
    if (rom) {
        path = rom->savePath;
        image.data = SaveFiles::read(path, image.size);
    }
    if (!image.data) {
        return std::nullopt;
    }
    SaveSource source;
    source.kind_ = Kind::SdFile;
    source.gameCode_ = game.code;
    source.sdPath_ = path;
    return source;
}

bool SaveSource::isCartridge() const {
    return kind_ == Kind::ArchiveGameCard || kind_ == Kind::DsCard;
}

std::optional<SaveImage> SaveSource::reread() const {
    SaveImage image;
    Result result = 0;
    switch (kind_) {
        case Kind::ArchiveGameCard:
            readArchive(titleId_, MEDIATYPE_GAME_CARD, image, result);
            break;
        case Kind::ArchiveSd:
            readArchive(titleId_, MEDIATYPE_SD, image, result);
            break;
        case Kind::SdFile:
            image.data = SaveFiles::read(sdPath_, image.size);
            break;
        case Kind::DsCard: {
            Cartridge::DsCardRead card = Cartridge::readDsSave(gameCode_, infrared_, result);
            image = {std::move(card.data), card.size};
            break;
        }
    }
    if (!image.data) {
        return std::nullopt;
    }
    return image;
}

bool SaveSource::write(const std::vector<std::uint8_t>& image, const std::vector<std::uint8_t>& previous) const {
    switch (kind_) {
        case Kind::ArchiveGameCard:
            return SaveArchive::write(titleId_, MEDIATYPE_GAME_CARD, image.data(), image.size());
        case Kind::ArchiveSd:
            return SaveArchive::write(titleId_, MEDIATYPE_SD, image.data(), image.size());
        case Kind::DsCard:
            return Cartridge::writeDsSave(cardType_, image.data(), image.size(),
                                           previous.empty() ? nullptr : previous.data(), previous.size());
        case Kind::SdFile:
            return SaveFiles::write(sdPath_, image.data(), image.size());
    }
    return false;
}
