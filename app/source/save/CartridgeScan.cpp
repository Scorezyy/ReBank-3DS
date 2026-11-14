#include "save/CartridgeScan.hpp"

#include "gui/icons/GameIconReader.hpp"
#include "save/storage/Cartridge.hpp"
#include "save/storage/TitleIds.hpp"

#include <3ds.h>

#include <algorithm>
#include <string>

namespace {
constexpr int PowerAttempts = 40;
constexpr s64 PowerPollNanoseconds = 25'000'000LL;
constexpr int IconAttempts = 10;
constexpr s64 IconRetryNanoseconds = 120'000'000LL;
constexpr int SummaryAttempts = 8;
constexpr s64 SummaryRetryNanoseconds = 100'000'000LL;

bool summaryLooksValid(const SaveSummary& summary) {
    return !summary.trainerName.empty() || summary.trainerId != 0 || summary.playTimeMinutes != 0;
}

std::optional<std::size_t> findGame(std::span<const GameDescriptor> games, GamePlatform platform,
                                    std::string_view code) {
    const auto match = std::find_if(games.begin(), games.end(), [&](const GameDescriptor& candidate) {
        return candidate.platform == platform && candidate.code == code;
    });
    if (match == games.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(match - games.begin());
}

std::optional<IdentifiedCartridge> identifyDs(std::span<const GameDescriptor> games) {
    const std::string code = Cartridge::dsGameCode();
    const std::optional<std::size_t> index = code.empty()
        ? std::nullopt
        : findGame(games, GamePlatform::NintendoDs, code);
    if (!index) {
        return std::nullopt;
    }
    return IdentifiedCartridge{*index, true, std::nullopt};
}

std::optional<IdentifiedCartridge> identify3ds(std::span<const GameDescriptor> games) {
    const std::optional<std::string_view> code = TitleIds::codeFor(Cartridge::titleId());
    const std::optional<std::size_t> index = code ? findGame(games, GamePlatform::Nintendo3Ds, *code) : std::nullopt;
    SaveSummary summary;
    if (!index || !CartridgeScan::readStableSummary(games[*index], summary)) {
        return std::nullopt;
    }
    return IdentifiedCartridge{*index, false, summary};
}
}

namespace CartridgeScan {
bool inserted() {
    bool present = false;
    FSUSER_CardSlotIsInserted(&present);
    return present;
}

bool waitForPower() {
    for (int attempt = 0; attempt < PowerAttempts; ++attempt) {
        bool powered = false;
        if (R_SUCCEEDED(FSUSER_CardSlotGetCardIFPowerStatus(&powered)) && powered) {
            return true;
        }
        svcSleepThread(PowerPollNanoseconds);
    }
    return false;
}

void resetSlotPower() {
    bool powerStatus = false;
    FSUSER_CardSlotPowerOff(&powerStatus);
    FSUSER_CardSlotPowerOn(&powerStatus);
    waitForPower();
}

std::optional<IdentifiedCartridge> identify(std::span<const GameDescriptor> games) {
    if (std::optional<IdentifiedCartridge> ds = identifyDs(games)) {
        return ds;
    }
    return identify3ds(games);
}

bool readStableIcon(const GameDescriptor& game, IconPixels& pixels) {
    IconPixels previous{};
    for (int attempt = 0; attempt < IconAttempts; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(IconRetryNanoseconds);
        }
        if (!GameIconReader::readUncached(game, true, pixels)) {
            continue;
        }
        if (attempt > 0 && previous == pixels) {
            return true;
        }
        previous = pixels;
    }
    return false;
}

bool readStableSummary(const GameDescriptor& game, SaveSummary& summary) {
    bool opened = false;
    for (int attempt = 0; attempt < SummaryAttempts; ++attempt) {
        if (attempt > 0) {
            svcSleepThread(SummaryRetryNanoseconds);
        }
        SaveAdapter candidate;
        std::string error;
        if (!candidate.open(game, error, SaveSource::Preference::CartridgeOnly)) {
            continue;
        }
        opened = true;
        summary = candidate.summary();
        if (summaryLooksValid(summary)) {
            return true;
        }
    }
    return opened;
}
}
