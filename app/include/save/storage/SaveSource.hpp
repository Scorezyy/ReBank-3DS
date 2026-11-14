#pragma once

#include "save/catalog/GameCatalog.hpp"
#include "spi.hpp"

#include <3ds.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct SaveImage {
    std::shared_ptr<std::uint8_t[]> data;
    std::size_t size = 0;
};

class SaveSource {
public:
    enum class Preference { Any, CartridgeOnly, StorageOnly };

    static std::optional<SaveSource> locate(const GameDescriptor& game, Preference preference, SaveImage& image,
                                            Result& result);
    static bool existsInStorage(const GameDescriptor& game);

    bool isCartridge() const;
    std::optional<SaveImage> reread() const;
    bool write(const std::vector<std::uint8_t>& image, const std::vector<std::uint8_t>& previous) const;
    int kindCode() const { return static_cast<int>(kind_); }

private:
    enum class Kind { ArchiveGameCard, ArchiveSd, DsCard, SdFile };

    static std::optional<SaveSource> locate3ds(const GameDescriptor& game, bool allowCartridge, bool allowStorage,
                                               SaveImage& image, Result& result);
    static std::optional<SaveSource> locateVirtualConsole(const GameDescriptor& game, SaveImage& image,
                                                          Result& result);
    static std::optional<SaveSource> locateDsCard(const GameDescriptor& game, SaveImage& image, Result& result);
    static std::optional<SaveSource> locateSdFile(const GameDescriptor& game, SaveImage& image);

    Kind kind_ = Kind::SdFile;
    std::string gameCode_;
    std::uint64_t titleId_ = 0;
    CardType cardType_ = NO_CHIP;
    bool infrared_ = false;
    std::string sdPath_;
};
