#include "gui/bankscreen/BankScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "gui/GameVisual.hpp"
#include "gui/elements/BoxBackground.hpp"
#include "gui/elements/Cursor.hpp"
#include "gui/elements/PokemonBadges.hpp"
#include "gui/elements/Shapes.hpp"
#include "gui/elements/TextMetrics.hpp"
#include "gui/Theme.hpp"
#include "network/ApiClient.hpp"

#include <enums/Species.hpp>
#include <utils/i18n.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace Gui;

void BankScreen::update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    if (commitJob_.running()) {
        return;
    }
    if (keysDown & KEY_B) {
        if (hand_.active) {
            storageReturnHand();
            return;
        }
        if (hasPendingChanges(true)) {
            discardPendingChanges();
            return;
        }
        if (storagePane_ == StoragePane::Cloud) {
            storagePane_ = StoragePane::Local;
        } else {
            app_.screen_ = App::Screen::GameSelect;
        }
        return;
    }

    const std::size_t boxLimit = app_.session_.boxLimit == 0 ? 50 : app_.session_.boxLimit;
    if ((keysDown & KEY_L) || (keysDown & KEY_R)) {
        if (hand_.active) {
            app_.status_ = "Drop the Pokemon first.";
        } else if (storagePane_ == StoragePane::Party) {
        } else if (keysDown & KEY_L) {
            if (storagePane_ == StoragePane::Cloud) {
                persistCloudDraft();
                cloudBox_ = cloudBox_ == 0 ? boxLimit - 1 : cloudBox_ - 1;
                refreshCloudBox();
            } else {
                persistLocalDraft();
                localBox_ = localBox_ == 0 ? saveAdapter_.boxCount() - 1 : localBox_ - 1;
                loadLocalBox();
            }
        } else {
            if (storagePane_ == StoragePane::Cloud) {
                persistCloudDraft();
                cloudBox_ = (cloudBox_ + 1) % boxLimit;
                refreshCloudBox();
            } else {
                persistLocalDraft();
                localBox_ = (localBox_ + 1) % saveAdapter_.boxCount();
                loadLocalBox();
            }
        }
    }

    if (cloudNameFocused_) {
        storageDirection(keysDown, keysHeld, circle);
        if (keysDown & (KEY_DOWN | KEY_B)) {
            cloudNameFocused_ = false;
        } else if (keysDown & KEY_A && !renameController_.isRunning()) {
            const auto position = static_cast<std::uint16_t>(cloudBox_ + 1);
            const auto cached = cloudBoxNames_.find(position);
            const std::string current = cached != cloudBoxNames_.end()
                ? cached->second
                : ("Bank " + std::to_string(position));
            std::string edited = current;
            app_.requestText(edited, "Box name", false);
            if (!edited.empty() && edited != current) {
                beginRenameBox(position, edited);
            }
        }
        return;
    }

    if (keysDown & KEY_A) {
        if (hand_.active) {
            storageDrop();
        } else {
            storagePickUp();
        }
    }

    auto move = [this](int direction) {
        if (direction == 0) {
            return;
        }
        if (storagePane_ == StoragePane::Party) {
            std::size_t column = focusedSlot_ % 2;
            std::size_t row = focusedSlot_ / 2;
            if (direction == 1) {
                row = row == 0 ? 0 : row - 1;
            } else if (direction == 2) {
                row = row == 2 ? 2 : row + 1;
            } else if (direction == 3) {
                column = column == 0 ? 0 : column - 1;
            } else if (direction == 4) {
                column = column == 1 ? 1 : column + 1;
            }
            focusedSlot_ = row * 2 + column;
            return;
        }
        std::size_t column = focusedSlot_ % 6;
        std::size_t row = focusedSlot_ / 6;
        if (direction == 1) {
            row = row == 0 ? 0 : row - 1;
        } else if (direction == 2) {
            row = row == 4 ? 4 : row + 1;
        } else if (direction == 3) {
            column = column == 0 ? 5 : column - 1;
        } else if (direction == 4) {
            column = (storagePane_ == StoragePane::Local && column == 5) ? 5 : (column + 1) % 6;
        }
        focusedSlot_ = row * 6 + column;
    };

    const std::size_t priorSlot = focusedSlot_;
    const StoragePane priorPane = storagePane_;
    move(storageDirection(keysDown, keysHeld, circle));

    if ((keysDown & KEY_UP)
        && priorPane == StoragePane::Local
        && storagePane_ == StoragePane::Local
        && priorSlot < 6
        && focusedSlot_ == priorSlot) {
        const std::size_t column = focusedSlot_ % 6;
        storagePane_ = StoragePane::Cloud;
        focusedSlot_ = 24 + column;
    } else if ((keysDown & KEY_DOWN)
        && priorPane == StoragePane::Cloud
        && storagePane_ == StoragePane::Cloud
        && priorSlot >= 24
        && focusedSlot_ == priorSlot) {
        const std::size_t column = focusedSlot_ % 6;
        storagePane_ = StoragePane::Local;
        focusedSlot_ = column;
    } else if ((keysDown & KEY_UP)
        && priorPane == StoragePane::Cloud
        && storagePane_ == StoragePane::Cloud
        && priorSlot < 6
        && focusedSlot_ == priorSlot
        && !hand_.active) {
        cloudNameFocused_ = true;
    } else if ((keysDown & KEY_RIGHT)
        && priorPane == StoragePane::Local
        && storagePane_ == StoragePane::Local
        && priorSlot % 6 == 5
        && focusedSlot_ == priorSlot) {
        const std::size_t row = focusedSlot_ / 6;
        storagePane_ = StoragePane::Party;
        focusedSlot_ = std::min<std::size_t>(row, 2) * 2;
    } else if ((keysDown & KEY_LEFT)
        && priorPane == StoragePane::Party
        && storagePane_ == StoragePane::Party
        && priorSlot % 2 == 0
        && focusedSlot_ == priorSlot) {
        const std::size_t row = focusedSlot_ / 2;
        storagePane_ = StoragePane::Local;
        focusedSlot_ = row * 6 + 5;
    }

    if (keysDown & KEY_SELECT) {
        if (hand_.active) {
            app_.status_ = "Drop the Pokemon first.";
        } else if (!hasPendingChanges()) {
            app_.status_ = "Nothing to commit.";
        } else if (app_.loadService_.running()) {
            commitRequested_ = true;
            app_.status_ = "Committing changes...";
        } else {
            beginCommit();
        }
    }

    pumpCommitRequest();
    pumpHandPayloadFetch();
    pumpCloudPayloadPrefetch();
    pumpCloudPrefetch();

    if (!touched) {
        return;
    }

    constexpr float gridX = 8.0F;
    constexpr float gridY = 60.0F;
    constexpr float pitchX = 32.0F;
    constexpr float pitchY = 25.0F;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float x = gridX + static_cast<float>(slot % 6) * pitchX;
        const float y = gridY + static_cast<float>(slot / 6) * pitchY;
        if (UiRect{x, y, pitchX, pitchY}.contains(touch)) {
            if (focusedSlot_ == slot && storagePane_ == StoragePane::Local) {
                if (hand_.active) {
                    storageDrop();
                } else {
                    storagePickUp();
                }
            } else {
                focusedSlot_ = slot;
                storagePane_ = StoragePane::Local;
            }
            return;
        }
    }

    constexpr float partyColAX = 244.0F;
    constexpr float partyColBX = 288.0F;
    constexpr float partyRowStep = 45.0F;
    constexpr float partyColATop = 86.0F;
    constexpr float partyColBTop = 108.0F;
    constexpr float partyTouchHalf = 18.0F;
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const std::size_t column = slot % 2;
        const std::size_t row = slot / 2;
        const float cx = column == 0 ? partyColAX : partyColBX;
        const float cy = (column == 0 ? partyColATop : partyColBTop) + static_cast<float>(row) * partyRowStep;
        const UiRect touchRect{cx - partyTouchHalf, cy - partyTouchHalf, partyTouchHalf * 2.0F, partyTouchHalf * 2.0F};
        if (touchRect.contains(touch)) {
            if (focusedSlot_ == slot && storagePane_ == StoragePane::Party) {
                if (hand_.active) {
                    storageDrop();
                } else {
                    storagePickUp();
                }
            } else {
                focusedSlot_ = slot;
                storagePane_ = StoragePane::Party;
            }
            return;
        }
    }
}

int BankScreen::storageDirection(u32 keysDown, u32 keysHeld, circlePosition circle) {
    int direction = 0;
    if ((keysHeld & KEY_UP) || circle.dy > 60) {
        direction = 1;
    } else if ((keysHeld & KEY_DOWN) || circle.dy < -60) {
        direction = 2;
    } else if ((keysHeld & KEY_LEFT) || circle.dx < -60) {
        direction = 3;
    } else if ((keysHeld & KEY_RIGHT) || circle.dx > 60) {
        direction = 4;
    }

    if (direction == 0) {
        heldDirection_ = 0;
        directionRepeatAt_ = 0;
        return 0;
    }
    const u64 now = svcGetSystemTick();
    const bool digitalPressed = (direction == 1 && (keysDown & KEY_UP))
        || (direction == 2 && (keysDown & KEY_DOWN))
        || (direction == 3 && (keysDown & KEY_LEFT))
        || (direction == 4 && (keysDown & KEY_RIGHT));
    if (direction != heldDirection_ || digitalPressed) {
        heldDirection_ = direction;
        directionRepeatAt_ = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.28);
        return direction;
    }
    if (now >= directionRepeatAt_) {
        directionRepeatAt_ = now + static_cast<u64>(SYSCLOCK_ARM11 * 0.09);
        return direction;
    }
    return 0;
}

int BankScreen::partyMemberCount() const {
    int count = 0;
    for (const PokemonSummary& member : partyWorking_.summaries) {
        if (member.species != 0) {
            ++count;
        }
    }
    return count;
}

void BankScreen::storagePickUp() {
    if (hand_.active) {
        return;
    }
    if (storagePane_ == StoragePane::Local) {
        const PokemonSummary& mon = storage_.pokemon(focusedSlot_);
        if (mon.species == 0) {
            app_.status_ = "This slot is empty.";
            return;
        }
        hand_.active = true;
        hand_.source = HandSource::Local;
        hand_.sourceIndex = focusedSlot_;
        hand_.summary = mon;
        hand_.payload = std::move(localPayloads_[focusedSlot_]);
        hand_.payloadKnown = !hand_.payload.data.empty();
        localPayloads_[focusedSlot_] = {};
        storage_.set(focusedSlot_, PokemonSummary{});
        app_.status_ = mon.nickname + " picked up.";
        return;
    }
    if (storagePane_ == StoragePane::Party) {
        const PokemonSummary& mon = partyWorking_.summaries[focusedSlot_];
        if (mon.species == 0) {
            app_.status_ = "This slot is empty.";
            return;
        }
        if (partyMemberCount() <= 1) {
            app_.status_ = "Your team can't be empty.";
            return;
        }
        hand_.active = true;
        hand_.source = HandSource::Party;
        hand_.sourceIndex = focusedSlot_;
        hand_.summary = mon;
        hand_.payload = std::move(partyWorking_.payloads[focusedSlot_]);
        hand_.payloadKnown = !hand_.payload.data.empty();
        partyWorking_.payloads[focusedSlot_] = {};
        partyWorking_.summaries[focusedSlot_] = PokemonSummary{};
        app_.status_ = mon.nickname + " picked up.";
        return;
    }

    const PokemonSummary& mon = cloudPreview_[focusedSlot_];
    if (mon.species == 0) {
        app_.status_ = "This slot is empty.";
        return;
    }
    if (app_.session_.accessToken.empty()) {
        app_.status_ = "Please sign in again.";
        return;
    }
    PokemonPayload payload;
    if (!pendingUploadPayloads_[focusedSlot_].data.empty()) {
        payload = std::move(pendingUploadPayloads_[focusedSlot_]);
        pendingUploadPayloads_[focusedSlot_] = {};
    } else if (!cachedCloudPayloads_[focusedSlot_].data.empty()) {
        payload = cachedCloudPayloads_[focusedSlot_];
    } else {
        hand_.active = true;
        hand_.source = HandSource::Cloud;
        hand_.sourceIndex = focusedSlot_;
        hand_.sourceCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
        hand_.summary = mon;
        hand_.payload = {};
        hand_.payloadKnown = false;
        cloudPreview_[focusedSlot_] = {};
        app_.status_ = mon.nickname + " picked up.";
        if (!app_.loadService_.running()) {
            app_.loadService_.pickupSlot = focusedSlot_;
            app_.loadService_.pickupCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
            app_.loadService_.pickupSummary = mon;
            app_.loadService_.begin(LoadService::Operation::PickupCloud);
        }
        return;
    }
    hand_.active = true;
    hand_.source = HandSource::Cloud;
    hand_.sourceIndex = focusedSlot_;
    hand_.sourceCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
    hand_.summary = mon;
    hand_.payload = std::move(payload);
    hand_.payloadKnown = !hand_.payload.data.empty();
    cloudPreview_[focusedSlot_] = {};
    app_.status_ = mon.nickname + " picked up.";
}

void BankScreen::storageDrop() {
    if (!hand_.active) {
        return;
    }
    if (!hand_.payloadKnown) {
        app_.status_ = "Still fetching " + hand_.summary.nickname + "...";
        return;
    }
    if (storagePane_ == StoragePane::Local) {
        const std::uint8_t saveGen = saveAdapter_.gameGeneration();
        if (!saveAdapter_.canImportPokemon(hand_.payload.format, hand_.payload.data)) {
            app_.status_ = "Gen " + std::to_string(hand_.payload.format)
                      + " cannot enter Gen " + std::to_string(saveGen) + ".";
            return;
        }
        const bool occupied = storage_.pokemon(focusedSlot_).species != 0;
        if (occupied) {
            PokemonSummary occupantSummary = storage_.pokemon(focusedSlot_);
            PokemonPayload occupantPayload = std::move(localPayloads_[focusedSlot_]);
            storage_.set(focusedSlot_, hand_.summary);
            localPayloads_[focusedSlot_] = hand_.payload;
            hand_.summary = std::move(occupantSummary);
            hand_.payload = std::move(occupantPayload);
            hand_.source = HandSource::Local;
            hand_.sourceIndex = focusedSlot_;
            hand_.payloadKnown = !hand_.payload.data.empty();
            app_.status_ = hand_.summary.nickname + " swapped.";
            return;
        }
        storage_.set(focusedSlot_, hand_.summary);
        localPayloads_[focusedSlot_] = hand_.payload;
        app_.status_ = hand_.summary.nickname + " placed.";
        hand_ = Hand{};
        return;
    }
    if (storagePane_ == StoragePane::Party) {
        const std::uint8_t saveGen = saveAdapter_.gameGeneration();
        if (!saveAdapter_.canImportPokemon(hand_.payload.format, hand_.payload.data)) {
            app_.status_ = "Gen " + std::to_string(hand_.payload.format)
                      + " cannot enter Gen " + std::to_string(saveGen) + ".";
            return;
        }
        const bool occupied = partyWorking_.summaries[focusedSlot_].species != 0;
        if (occupied) {
            PokemonSummary occupantSummary = partyWorking_.summaries[focusedSlot_];
            PokemonPayload occupantPayload = std::move(partyWorking_.payloads[focusedSlot_]);
            partyWorking_.summaries[focusedSlot_] = hand_.summary;
            partyWorking_.payloads[focusedSlot_] = hand_.payload;
            hand_.summary = std::move(occupantSummary);
            hand_.payload = std::move(occupantPayload);
            hand_.source = HandSource::Party;
            hand_.sourceIndex = focusedSlot_;
            hand_.payloadKnown = !hand_.payload.data.empty();
            app_.status_ = hand_.summary.nickname + " swapped.";
            return;
        }
        partyWorking_.summaries[focusedSlot_] = hand_.summary;
        partyWorking_.payloads[focusedSlot_] = hand_.payload;
        app_.status_ = hand_.summary.nickname + " placed.";
        hand_ = Hand{};
        return;
    }

    const bool occupied = cloudPreview_[focusedSlot_].species != 0;
    if (occupied) {
        PokemonPayload occupantPayload;
        if (!pendingUploadPayloads_[focusedSlot_].data.empty()) {
            occupantPayload = std::move(pendingUploadPayloads_[focusedSlot_]);
            pendingUploadPayloads_[focusedSlot_] = {};
        } else if (!cachedCloudPayloads_[focusedSlot_].data.empty()) {
            occupantPayload = cachedCloudPayloads_[focusedSlot_];
        } else if (!app_.session_.accessToken.empty()) {
            if (app_.loadService_.running()) {
                return;
            }
            app_.loadService_.pickupSlot = focusedSlot_;
            app_.loadService_.pickupCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
            app_.loadService_.pickupSummary = cloudPreview_[focusedSlot_];
            app_.status_ = "Fetching occupant...";
            app_.loadService_.begin(LoadService::Operation::SwapCloud);
            return;
        } else {
            app_.status_ = "Please sign in again.";
            return;
        }
        PokemonSummary occupantSummary = cloudPreview_[focusedSlot_];
        cloudPreview_[focusedSlot_] = hand_.summary;
        pendingUploadPayloads_[focusedSlot_] = hand_.payload;
        hand_.summary = std::move(occupantSummary);
        hand_.payload = std::move(occupantPayload);
        hand_.source = HandSource::Cloud;
        hand_.sourceIndex = focusedSlot_;
        hand_.sourceCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
        hand_.payloadKnown = !hand_.payload.data.empty();
        app_.status_ = hand_.summary.nickname + " swapped.";
        return;
    }
    cloudPreview_[focusedSlot_] = hand_.summary;
    PokemonPayload payload = hand_.payload;
    pendingUploadPayloads_[focusedSlot_] = std::move(payload);
    app_.status_ = hand_.summary.nickname + " placed.";
    hand_ = Hand{};
}

void BankScreen::storageReturnHand() {
    if (!hand_.active) {
        return;
    }
    if (hand_.source == HandSource::Local) {
        storage_.set(hand_.sourceIndex, hand_.summary);
        localPayloads_[hand_.sourceIndex] = hand_.payload;
    } else if (hand_.source == HandSource::Party) {
        partyWorking_.summaries[hand_.sourceIndex] = hand_.summary;
        partyWorking_.payloads[hand_.sourceIndex] = hand_.payload;
    } else {
        cloudPreview_[hand_.sourceIndex] = hand_.summary;
        if (!hand_.payload.data.empty()) {
            cachedCloudPayloads_[hand_.sourceIndex] = hand_.payload;
        }
    }
    app_.status_ = "Returned to slot " + std::to_string(hand_.sourceIndex + 1) + ".";
    hand_ = Hand{};
}

bool BankScreen::hasPendingChanges(bool verbose) const {
    for (const auto& [box, draft] : localDrafts_) {
        auto it = localBaselines_.find(box);
        if (it == localBaselines_.end()) {
            if (verbose) {
                Logger::instance().info("hasPendingChanges: localDrafts_ box "
                                        + std::to_string(box) + " has no baseline");
            }
            return true;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (draft.summaries[slot].species != it->second.summaries[slot].species
                || draft.summaries[slot].nickname != it->second.summaries[slot].nickname
                || draft.payloads[slot].data != it->second.payloads[slot].data) {
                if (verbose) {
                    Logger::instance().info("hasPendingChanges: localDrafts_ box "
                                            + std::to_string(box) + " slot " + std::to_string(slot)
                                            + " differs from baseline");
                }
                return true;
            }
        }
    }
    if (localBox_ < saveAdapter_.boxCount()) {
        auto it = localBaselines_.find(localBox_);
        if (it != localBaselines_.end()) {
            for (std::size_t slot = 0; slot < 30; ++slot) {
                if (storage_.pokemon(slot).species != it->second.summaries[slot].species
                    || storage_.pokemon(slot).nickname != it->second.summaries[slot].nickname
                    || localPayloads_[slot].data != it->second.payloads[slot].data) {
                    if (verbose) {
                        Logger::instance().info("hasPendingChanges: local box "
                                                + std::to_string(localBox_) + " slot " + std::to_string(slot)
                                                + " differs from baseline (species "
                                                + std::to_string(storage_.pokemon(slot).species) + " vs "
                                                + std::to_string(it->second.summaries[slot].species)
                                                + ", payload " + std::to_string(localPayloads_[slot].data.size())
                                                + " vs " + std::to_string(it->second.payloads[slot].data.size()) + " bytes)");
                    }
                    return true;
                }
            }
        }
    }
    for (const auto& [box, draft] : cloudBoxes_) {
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (draft.summaries[slot].species != draft.baseline[slot].species
                || draft.summaries[slot].nickname != draft.baseline[slot].nickname
                || !draft.pending[slot].data.empty()) {
                if (verbose) {
                    Logger::instance().info("hasPendingChanges: cloud box "
                                            + std::to_string(box) + " slot " + std::to_string(slot)
                                            + " differs from baseline or has pending upload");
                }
                return true;
            }
        }
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (!pendingUploadPayloads_[slot].data.empty()) {
            if (verbose) {
                Logger::instance().info("hasPendingChanges: pendingUploadPayloads_ slot "
                                        + std::to_string(slot) + " non-empty");
            }
            return true;
        }
    }
    for (std::size_t slot = 0; slot < 6; ++slot) {
        if (partyWorking_.summaries[slot].species != partyBaseline_.summaries[slot].species
            || partyWorking_.summaries[slot].nickname != partyBaseline_.summaries[slot].nickname
            || partyWorking_.payloads[slot].data != partyBaseline_.payloads[slot].data) {
            if (verbose) {
                Logger::instance().info("hasPendingChanges: party slot " + std::to_string(slot)
                                        + " differs from baseline (species "
                                        + std::to_string(partyWorking_.summaries[slot].species) + " vs "
                                        + std::to_string(partyBaseline_.summaries[slot].species)
                                        + ", payload " + std::to_string(partyWorking_.payloads[slot].data.size())
                                        + " vs " + std::to_string(partyBaseline_.payloads[slot].data.size()) + " bytes)");
            }
            return true;
        }
    }
    return false;
}

void BankScreen::loadLocalBox() {
    localBoxName_ = saveAdapter_.boxName(localBox_);
    storagePane_ = StoragePane::Local;
    focusedSlot_ = 0;

    if (localBaselines_.find(localBox_) == localBaselines_.end()) {
        LocalBoxDraft baseline;
        const BoxRead read = saveAdapter_.readBoxFull(localBox_);
        baseline.summaries = read.summaries;
        baseline.payloads = read.payloads;
        localBaselines_[localBox_] = std::move(baseline);
    }

    auto draftIt = localDrafts_.find(localBox_);
    if (draftIt != localDrafts_.end()) {
        storage_.load(draftIt->second.summaries);
        localPayloads_ = draftIt->second.payloads;
    } else {
        const LocalBoxDraft& baseline = localBaselines_[localBox_];
        storage_.load(baseline.summaries);
        localPayloads_ = baseline.payloads;
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (storage_.pokemon(slot).species != 0) {
            focusedSlot_ = slot;
            break;
        }
    }
    Logger::instance().info("Local box loaded: " + std::to_string(localBox_ + 1));
}

void BankScreen::persistLocalDraft() {
    auto baselineIt = localBaselines_.find(localBox_);
    if (baselineIt == localBaselines_.end()) {
        return;
    }
    LocalBoxDraft current;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        current.summaries[slot] = storage_.pokemon(slot);
        current.payloads[slot] = localPayloads_[slot];
    }
    bool differs = false;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (current.summaries[slot].species != baselineIt->second.summaries[slot].species
            || current.summaries[slot].nickname != baselineIt->second.summaries[slot].nickname
            || current.payloads[slot].data != baselineIt->second.payloads[slot].data) {
            differs = true;
            break;
        }
    }
    if (differs) {
        localDrafts_[localBox_] = std::move(current);
    } else {
        localDrafts_.erase(localBox_);
    }
}

bool BankScreen::nextCloudPrefetchKey(std::uint16_t& outKey) const {
    if (app_.session_.accessToken.empty()) {
        return false;
    }
    const std::size_t boxLimit = app_.session_.boxLimit == 0 ? 50 : app_.session_.boxLimit;
    const u64 now = svcGetSystemTick();

    const auto onCooldown = [&](std::uint16_t key) {
        const auto it = cloudPrefetchCooldownUntil_.find(key);
        return it != cloudPrefetchCooldownUntil_.end() && now < it->second;
    };
    const auto centerKey = static_cast<std::uint16_t>(cloudBox_);
    if (!cloudBoxes_.count(centerKey) && !onCooldown(centerKey)) {
        outKey = centerKey;
        return true;
    }
    constexpr std::size_t maxPrefetchDistance = 3;
    for (std::size_t distance = 1; distance <= maxPrefetchDistance && distance < boxLimit; ++distance) {
        const auto leftKey = static_cast<std::uint16_t>((cloudBox_ + boxLimit - distance) % boxLimit);
        if (!cloudBoxes_.count(leftKey) && !onCooldown(leftKey)) {
            outKey = leftKey;
            return true;
        }
        const auto rightKey = static_cast<std::uint16_t>((cloudBox_ + distance) % boxLimit);
        if (rightKey != leftKey && !cloudBoxes_.count(rightKey) && !onCooldown(rightKey)) {
            outKey = rightKey;
            return true;
        }
    }
    return false;
}

void BankScreen::pumpHandPayloadFetch() {
    if (!hand_.active || hand_.source != HandSource::Cloud || hand_.payloadKnown) {
        return;
    }
    if (app_.loadService_.running()) {
        return;
    }
    app_.loadService_.pickupSlot = hand_.sourceIndex;
    app_.loadService_.pickupCloudBox = hand_.sourceCloudBox;
    app_.loadService_.pickupSummary = hand_.summary;
    app_.loadService_.begin(LoadService::Operation::PickupCloud);
}

void BankScreen::pumpCloudPayloadPrefetch() {
    if (hand_.active || storagePane_ != StoragePane::Cloud || commitRequested_) {
        return;
    }
    if (app_.loadService_.running() || renameController_.isRunning() || commitJob_.running()) {
        return;
    }
    for (std::size_t slot = 0; slot < cloudPreview_.size(); ++slot) {
        if (cloudPreview_[slot].species == 0) {
            continue;
        }
        if (!cachedCloudPayloads_[slot].data.empty() || payloadPrefetchFailed_[slot]) {
            continue;
        }
        if (!pendingUploadPayloads_[slot].data.empty()) {
            continue;
        }
        Logger::instance().info("pumpCloudPayloadPrefetch: starting fetch for box "
                                + std::to_string(cloudBox_ + 1) + " slot " + std::to_string(slot + 1));
        app_.loadService_.pickupSlot = slot;
        app_.loadService_.pickupCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
        app_.loadService_.pickupSummary = cloudPreview_[slot];
        app_.loadService_.begin(LoadService::Operation::PickupCloud);
        return;
    }
}

void BankScreen::pumpCommitRequest() {
    if (!commitRequested_) {
        return;
    }
    if (app_.loadService_.running()) {
        return;
    }
    commitRequested_ = false;
    if (hand_.active) {
        app_.status_ = "Drop the Pokemon first.";
        return;
    }
    if (!hasPendingChanges()) {
        app_.status_ = "Nothing to commit.";
        return;
    }
    beginCommit();
}

void BankScreen::pumpCloudPrefetch() {
    if (hand_.active || commitRequested_ || storagePane_ != StoragePane::Cloud) {
        return;
    }
    if (app_.loadService_.running() || renameController_.isRunning() || commitJob_.running()) {
        return;
    }
    std::uint16_t key = 0;
    if (!nextCloudPrefetchKey(key)) {
        return;
    }
    Logger::instance().info("pumpCloudPrefetch: starting fetch for box "
                            + std::to_string(key + 1));
    app_.loadService_.cloudBoxKey = key;
    app_.loadService_.begin(LoadService::Operation::CloudBox);
}

void BankScreen::refreshCloudBox(bool keepPreviousPreview) {
    const auto boxKey = static_cast<std::uint16_t>(cloudBox_);
    if (app_.session_.accessToken.empty()) {
        cloudPreview_.fill({});
        pendingUploadPayloads_ = {};
        return;
    }

    auto it = cloudBoxes_.find(boxKey);
    if (it == cloudBoxes_.end()) {
        if (!keepPreviousPreview) {
            cloudPreview_.fill({});
            pendingUploadPayloads_ = {};
        }
        cachedCloudPayloads_ = {};
        payloadPrefetchFailed_ = {};
        app_.status_.clear();
        app_.loadService_.cloudBoxKey = boxKey;
        app_.loadService_.begin(LoadService::Operation::CloudBox);
        return;
    }
    cloudPreview_ = it->second.summaries;
    pendingUploadPayloads_ = it->second.pending;
    cachedCloudPayloads_ = it->second.payloads;
    payloadPrefetchFailed_ = {};
    std::size_t occupied = 0;
    for (const auto& mon : cloudPreview_) {
        if (mon.species != 0) {
            ++occupied;
        }
    }
    Logger::instance().info("Cloud box " + std::to_string(cloudBox_ + 1)
                            + " loaded (" + std::to_string(occupied) + " occupied)");
}

void BankScreen::persistCloudDraft() {
    const auto boxKey = static_cast<std::uint16_t>(cloudBox_);
    auto& draft = cloudBoxes_[boxKey];
    draft.summaries = cloudPreview_;
    draft.pending = pendingUploadPayloads_;
}

void BankScreen::discardPendingChanges() {
    const StoragePane previousPane = storagePane_;
    hand_ = Hand{};
    localDrafts_.clear();
    localBaselines_.clear();
    for (auto& [key, draft] : cloudBoxes_) {
        draft.summaries = draft.baseline;
        draft.pending = {};
    }
    pendingUploadPayloads_ = {};
    cachedCloudPayloads_ = {};
    payloadPrefetchFailed_ = {};
    partyWorking_ = partyBaseline_;
    loadLocalBox();
    storagePane_ = previousPane;
    refreshCloudBox();
    app_.status_ = "Pending changes discarded.";
    Logger::instance().info("Pending storage changes discarded, cloudBox="
                            + std::to_string(cloudBox_ + 1) + " loaderRunning="
                            + std::to_string(app_.loadService_.running()));
}

void BankScreen::beginRenameBox(std::uint16_t position, std::string name) {
    if (renameController_.isRunning()) {
        return;
    }
    if (app_.session_.accessToken.empty()) {
        app_.status_ = "Please sign in again.";
        return;
    }
    renameController_.begin(app_.api_, position, std::move(name), app_.session_.accessToken);
}

void BankScreen::pollRenameBox() {
    std::uint16_t position = 0;
    RenameBoxResult result;
    if (!renameController_.poll(position, result)) {
        return;
    }
    if (result.success) {
        cloudBoxNames_[position] = result.name;
    }
}

void BankScreen::beginCommit() {
    if (commitJob_.running()) {
        return;
    }
    if (app_.session_.accessToken.empty()) {
        app_.status_ = "Please sign in again.";
        return;
    }
    persistLocalDraft();
    persistCloudDraft();
    commitPhase_.store(0, std::memory_order_release);
    commitProgress_.store(0, std::memory_order_release);
    app_.status_ = "Committing changes...";
    if (!commitJob_.start([this]() { commitWorker(this); })) {
        app_.status_ = "Could not start the commit.";
    }
}

void BankScreen::commitWorker(void* argument) {
    auto* self = static_cast<BankScreen*>(argument);
    CommitResult result;

    std::vector<UploadPokemon> uploads;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> deletes;
    for (const auto& [boxKey, draft] : self->cloudBoxes_) {
        const auto boxPosition = static_cast<std::uint16_t>(boxKey + 1);
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool initHas = draft.baseline[slot].species != 0;
            const bool nowHas = draft.summaries[slot].species != 0;
            const bool same = draft.summaries[slot].species == draft.baseline[slot].species
                && draft.summaries[slot].nickname == draft.baseline[slot].nickname;
            if (nowHas && (!same || !draft.pending[slot].data.empty())) {
                const auto& payload = draft.pending[slot];
                if (payload.data.empty()) {
                    result.message = "A staged Pokemon is missing its payload.";
                    self->commitResult_ = result;
                    return;
                }
                uploads.push_back(UploadPokemon{
                    boxPosition,
                    static_cast<std::uint8_t>(slot + 1),
                    payload.format,
                    payload.data,
                    draft.summaries[slot].species,
                    draft.summaries[slot].nickname,
                    draft.summaries[slot].trainerName,
                    draft.summaries[slot].level,
                    draft.summaries[slot].gameCode,
                    draft.summaries[slot].shiny,
                    draft.summaries[slot].heldItem
                });
            }
            if (initHas && !nowHas) {
                deletes.emplace_back(boxPosition, static_cast<std::uint8_t>(slot + 1));
            }
        }
    }

    std::size_t localChangeCount = 0;
    for (const auto& [boxKey, draft] : self->localDrafts_) {
        const auto baselineIt = self->localBaselines_.find(boxKey);
        if (baselineIt == self->localBaselines_.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool sameSummary = draft.summaries[slot].species
                    == baselineIt->second.summaries[slot].species
                && draft.summaries[slot].nickname
                    == baselineIt->second.summaries[slot].nickname;
            const bool samePayload = draft.payloads[slot].data
                == baselineIt->second.payloads[slot].data;
            if (!sameSummary || !samePayload) {
                ++localChangeCount;
            }
        }
    }
    std::size_t partyChangeCount = 0;
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const bool sameSummary = self->partyWorking_.summaries[slot].species
                == self->partyBaseline_.summaries[slot].species
            && self->partyWorking_.summaries[slot].nickname
                == self->partyBaseline_.summaries[slot].nickname;
        const bool samePayload = self->partyWorking_.payloads[slot].data
            == self->partyBaseline_.payloads[slot].data;
        if (!sameSummary || !samePayload) {
            ++partyChangeCount;
        }
    }
    const std::size_t uploadBatchCount = (uploads.size() + 29) / 30;
    const std::size_t totalSteps = localChangeCount + partyChangeCount
        + (localChangeCount + partyChangeCount > 0 ? 1 : 0)
        + deletes.size() + uploadBatchCount;
    std::size_t completedSteps = 0;
    const auto advanceProgress = [&]() {
        ++completedSteps;
        const int percent = totalSteps == 0
            ? 100
            : static_cast<int>((completedSteps * 100) / totalSteps);
        self->commitProgress_.store(percent, std::memory_order_release);
    };

    while (!uploads.empty()) {
        self->commitPhase_.store(2, std::memory_order_release);
        const std::size_t batchSize = std::min<std::size_t>(uploads.size(), 30);
        std::vector<UploadPokemon> batch(uploads.begin(), uploads.begin() + batchSize);
        uploads.erase(uploads.begin(), uploads.begin() + batchSize);
        UploadResult ur = self->app_.api_.uploadPokemon(batch, self->app_.session_.accessToken);
        if (!ur.success) {
            result.message = "Upload failed: " + ur.message;
            result.problemReason = ur.message;
            std::uint16_t problemBank = 0;
            const std::size_t bankMarker = ur.message.find("Bank ");
            if (bankMarker != std::string::npos) {
                std::size_t begin = bankMarker + 5;
                std::size_t end = begin;
                while (end < ur.message.size() && ur.message[end] >= '0'
                       && ur.message[end] <= '9') {
                    problemBank = static_cast<std::uint16_t>(
                        problemBank * 10 + (ur.message[end] - '0'));
                    ++end;
                }
            }
            const std::size_t marker = ur.message.find("Slot ");
            if (marker != std::string::npos) {
                std::size_t begin = marker + 5;
                std::size_t end = begin;
                std::uint8_t problemSlot = 0;
                while (end < ur.message.size() && ur.message[end] >= '0'
                       && ur.message[end] <= '9') {
                    problemSlot = static_cast<std::uint8_t>(
                        problemSlot * 10 + (ur.message[end] - '0'));
                    ++end;
                }
                if (end > begin) {
                    const auto problem = std::find_if(batch.begin(), batch.end(),
                        [problemBank, problemSlot](const UploadPokemon& upload) {
                            return upload.slot == problemSlot
                                && (problemBank == 0 || upload.boxPosition == problemBank);
                        });
                    if (problem != batch.end()) {
                        result.problemPokemon = problem->nickname.empty()
                            ? "Pokemon #" + std::to_string(problem->species)
                            : problem->nickname;
                        result.problemLocation = "Bank "
                            + std::to_string(problem->boxPosition) + "  |  Slot "
                            + std::to_string(problem->slot);
                    }
                    const std::size_t reason = ur.message.find(':', end);
                    if (reason != std::string::npos && reason + 1 < ur.message.size()) {
                        result.problemReason = ur.message.substr(reason + 1);
                        while (!result.problemReason.empty()
                               && result.problemReason.front() == ' ') {
                            result.problemReason.erase(result.problemReason.begin());
                        }
                    }
                }
            }
            self->commitResult_ = result;
            return;
        }
        result.uploads += ur.storedCount;
        advanceProgress();
    }

    bool anyLocalWrite = false;
    struct LocalWriteVerification {
        std::size_t box;
        std::size_t slot;
        std::uint16_t species;
    };
    std::vector<LocalWriteVerification> localWriteVerifications;
    for (const auto& [boxKey, draft] : self->localDrafts_) {
        auto baselineIt = self->localBaselines_.find(boxKey);
        if (baselineIt == self->localBaselines_.end()) {
            continue;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            const bool sameSummary = draft.summaries[slot].species == baselineIt->second.summaries[slot].species
                && draft.summaries[slot].nickname == baselineIt->second.summaries[slot].nickname;
            const bool samePayload = draft.payloads[slot].data == baselineIt->second.payloads[slot].data;
            if (sameSummary && samePayload) {
                continue;
            }
            if (draft.summaries[slot].species == 0) {
                if (!self->saveAdapter_.clearSlot(boxKey, slot)) {
                    result.message = "Could not clear local slot " + std::to_string(slot + 1) + ".";
                    self->commitResult_ = result;
                    return;
                }
            } else if (!draft.payloads[slot].data.empty()) {
                if (!self->saveAdapter_.writePokemon(boxKey, slot, draft.payloads[slot].format, draft.payloads[slot].data)) {
                    result.message = "Local write failed for slot " + std::to_string(slot + 1) + " (incompatible generation).";
                    self->commitResult_ = result;
                    return;
                }
                localWriteVerifications.push_back({
                    boxKey, slot, draft.summaries[slot].species
                });
                ++result.downloads;
            }
            anyLocalWrite = true;
            advanceProgress();
        }
    }

    std::vector<std::size_t> partyWriteVerificationSlots;
    std::vector<std::uint16_t> partyWriteVerificationSpecies;
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto& summary = self->partyWorking_.summaries[slot];
        const auto& baseline = self->partyBaseline_.summaries[slot];
        const bool sameSummary = summary.species == baseline.species
            && summary.nickname == baseline.nickname;
        const bool samePayload = self->partyWorking_.payloads[slot].data
            == self->partyBaseline_.payloads[slot].data;
        if (sameSummary && samePayload) {
            continue;
        }
        if (summary.species == 0) {
            if (!self->saveAdapter_.clearPartySlot(slot)) {
                result.message = "Could not clear party slot " + std::to_string(slot + 1) + ".";
                self->commitResult_ = result;
                return;
            }
        } else if (!self->partyWorking_.payloads[slot].data.empty()) {
            const auto& payload = self->partyWorking_.payloads[slot];
            if (!self->saveAdapter_.writePartyPokemon(slot, payload.format, payload.data)) {
                result.message = "Party write failed for slot " + std::to_string(slot + 1) + " (incompatible generation).";
                self->commitResult_ = result;
                return;
            }
            partyWriteVerificationSlots.push_back(slot);
            partyWriteVerificationSpecies.push_back(summary.species);
            ++result.downloads;
        }
        anyLocalWrite = true;
        advanceProgress();
    }

    if (anyLocalWrite) {
        self->commitPhase_.store(3, std::memory_order_release);
        std::string saveError;
        if (!self->saveAdapter_.writeSave(saveError)) {
            result.message = "Save write failed: " + saveError;
            self->commitResult_ = result;
            return;
        }
        advanceProgress();
        for (const auto& expected : localWriteVerifications) {
            const auto savedBox = self->saveAdapter_.readBox(expected.box);
            if (expected.slot >= savedBox.size()
                || savedBox[expected.slot].species != expected.species) {
                result.message = "Saved Pokemon verification failed; cloud copy retained.";
                Logger::instance().error("Post-save verification failed for box "
                                         + std::to_string(expected.box + 1) + " slot "
                                         + std::to_string(expected.slot + 1));
                self->commitResult_ = result;
                return;
            }
        }
        if (!partyWriteVerificationSlots.empty()) {
            const auto savedParty = self->saveAdapter_.readParty();
            for (std::size_t i = 0; i < partyWriteVerificationSlots.size(); ++i) {
                const std::size_t slot = partyWriteVerificationSlots[i];
                if (slot >= savedParty.size() || savedParty[slot].species != partyWriteVerificationSpecies[i]) {
                    result.message = "Saved Pokemon verification failed; cloud copy retained.";
                    Logger::instance().error("Post-save party verification failed for slot "
                                             + std::to_string(slot + 1));
                    self->commitResult_ = result;
                    return;
                }
            }
        }
    }

    for (const auto& deletion : deletes) {
        self->commitPhase_.store(1, std::memory_order_release);
        DeleteResult dr = self->app_.api_.deleteCloudPokemon(deletion.first, deletion.second, self->app_.session_.accessToken);
        if (!dr.success) {
            result.message = "Delete failed: " + dr.message;
            self->commitResult_ = result;
            return;
        }
        ++result.deletes;
        advanceProgress();
    }

    result.success = true;
    self->commitProgress_.store(100, std::memory_order_release);
    result.message = "Uploaded " + std::to_string(result.uploads)
                     + ", removed " + std::to_string(result.deletes)
                     + ", saved locally.";
    self->commitResult_ = result;
}

void BankScreen::pollCommit() {
    if (!commitJob_.poll()) {
        return;
    }
    if (commitResult_.success) {
        app_.status_ = commitResult_.message;
        const StoragePane previousPane = storagePane_;
        const std::size_t previousSlot = focusedSlot_;
        localBaselines_.clear();
        localDrafts_.clear();
        for (auto& [key, draft] : cloudBoxes_) {
            draft.baseline = draft.summaries;
            draft.pending = {};
        }
        loadLocalBox();
        refreshCloudBox(true);
        storagePane_ = previousPane;
        focusedSlot_ = previousSlot;
        partyBaseline_.summaries = saveAdapter_.readParty();
        for (std::size_t slot = 0; slot < 6; ++slot) {
            partyBaseline_.payloads[slot] = saveAdapter_.readPartyPokemon(slot);
        }
        partyWorking_ = partyBaseline_;
        return;
    }
    Logger::instance().warning("Commit failed: " + commitResult_.message);
    const std::string failure = commitResult_.message;
    discardPendingChanges();
    app_.status_ = failure + " Changes reloaded.";
    errorDialogTitle_ = "TRANSFER BLOCKED";
    errorDialogPokemon_ = commitResult_.problemPokemon.empty()
        ? "Transfer failed"
        : commitResult_.problemPokemon;
    errorDialogLocation_ = commitResult_.problemLocation;
    errorDialogMessage_ = commitResult_.problemReason.empty()
        ? failure
        : commitResult_.problemReason;
    errorDialogVisible_ = true;
}

void BankScreen::onGameOpened() {
    LoadService::OpenGameResult& result = app_.loadService_.openGameResult;
    saveSummary_ = std::move(result.save);
    localBox_ = result.localBox;
    cloudBox_ = 0;
    localBoxName_ = std::move(result.localBoxName);
    localBaselines_.clear();
    localDrafts_.clear();
    LocalBoxDraft baseline;
    baseline.summaries = std::move(result.localPokemon);
    baseline.payloads = std::move(result.localPayloads);
    localBaselines_[localBox_] = std::move(baseline);
    storage_.load(localBaselines_[localBox_].summaries);
    localPayloads_ = localBaselines_[localBox_].payloads;
    partyBaseline_.summaries = result.localParty;
    partyBaseline_.payloads = result.localPartyPayloads;
    partyWorking_ = partyBaseline_;
    storagePane_ = StoragePane::Local;
    focusedSlot_ = 0;
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (storage_.pokemon(slot).species != 0) {
            focusedSlot_ = slot;
            break;
        }
    }
    cloudBoxNames_.clear();
    for (const auto& entry : app_.cloudBoxNamesCache_) {
        cloudBoxNames_[entry.position] = entry.name;
    }

    const auto existingBox0 = cloudBoxes_.find(0);
    if (existingBox0 != cloudBoxes_.end()) {
        cloudPreview_ = existingBox0->second.summaries;
        cachedCloudPayloads_ = existingBox0->second.payloads;
        app_.status_.clear();
    } else if (app_.cloudBoxCache_.success) {
        CloudBoxDraft cloud;
        cloud.baseline = app_.cloudBoxCache_.pokemon;
        cloud.summaries = app_.cloudBoxCache_.pokemon;
        cloud.payloads = app_.cloudBoxCache_.payloads;
        cloudBoxes_[0] = cloud;
        cloudPreview_ = cloud.summaries;
        cachedCloudPayloads_ = cloud.payloads;
        app_.status_.clear();
    } else {
        cloudPreview_.fill({});
        cachedCloudPayloads_ = {};
        app_.status_.clear();
    }
    app_.cloudBoxCache_ = {};
    pendingUploadPayloads_ = {};
    payloadPrefetchFailed_ = {};
    hand_ = Hand{};
    app_.screen_ = App::Screen::Bank;
}

void BankScreen::onCloudBoxLoaded() {
    const auto boxKey = app_.loadService_.resolvedCloudBoxKey;
    const BoxListResult& result = app_.loadService_.cloudBoxResult;
    Logger::instance().info("onCloudBoxLoaded: box " + std::to_string(boxKey + 1)
                            + " success=" + std::to_string(result.success)
                            + " currentCloudBox=" + std::to_string(cloudBox_ + 1));
    if (result.success) {
        CloudBoxDraft draft;
        draft.baseline = result.pokemon;
        draft.summaries = result.pokemon;
        draft.payloads = result.payloads;
        cloudBoxes_[boxKey] = std::move(draft);
        cloudPrefetchCooldownUntil_.erase(boxKey);
        if (cloudBox_ == boxKey) {
            cloudPreview_ = cloudBoxes_[boxKey].summaries;
            pendingUploadPayloads_ = cloudBoxes_[boxKey].pending;
            cachedCloudPayloads_ = cloudBoxes_[boxKey].payloads;
            payloadPrefetchFailed_ = {};
        }
        app_.status_.clear();
        return;
    }
    if (cloudBox_ == boxKey) {
        cloudPreview_.fill({});
        pendingUploadPayloads_ = {};
    }

    cloudPrefetchCooldownUntil_[boxKey] = svcGetSystemTick() + static_cast<u64>(15.0 * SYSCLOCK_ARM11);
    app_.status_ = result.message;
    Logger::instance().warning("Cloud box refresh failed: " + app_.status_);
}

void BankScreen::onCloudPickupCompleted() {
    const bool stillHeld = hand_.active
        && hand_.source == HandSource::Cloud
        && hand_.sourceIndex == app_.loadService_.pickupSlot
        && !hand_.payloadKnown;
    DownloadResult& result = app_.loadService_.pickupResult;
    if (result.success) {
        PokemonPayload payload;
        payload.format = result.pokemon.format;
        payload.data = std::move(result.pokemon.payload);
        cachedCloudPayloads_[app_.loadService_.pickupSlot] = payload;
        if (stillHeld) {
            hand_.payload = std::move(payload);
            hand_.payloadKnown = !hand_.payload.data.empty();
        }
        return;
    }
    if (!stillHeld) {
        if (app_.loadService_.pickupCloudBox == static_cast<std::uint16_t>(cloudBox_ + 1)
            && app_.loadService_.pickupSlot < payloadPrefetchFailed_.size()) {
            payloadPrefetchFailed_[app_.loadService_.pickupSlot] = true;
        }
        Logger::instance().warning("Cloud payload prefetch failed: " + result.message);
        return;
    }
    cloudPreview_[app_.loadService_.pickupSlot] = hand_.summary;
    hand_ = Hand{};
    app_.status_ = "Cannot pick up: " + result.message;
    errorDialogTitle_ = "PICKUP FAILED";
    errorDialogPokemon_ = app_.loadService_.pickupSummary.nickname.empty()
        ? "Unknown Pokemon"
        : app_.loadService_.pickupSummary.nickname;
    errorDialogLocation_.clear();
    errorDialogMessage_ = result.message.empty()
        ? "The Pokemon payload could not be read."
        : result.message;
    errorDialogVisible_ = true;
    Logger::instance().warning("Cloud pickup failed: " + result.message);
}

void BankScreen::onCloudSwapCompleted() {
    DownloadResult& result = app_.loadService_.pickupResult;
    if (!result.success) {
        app_.status_ = "Cannot swap: " + result.message;
        errorDialogTitle_ = "SWAP FAILED";
        errorDialogPokemon_ = app_.loadService_.pickupSummary.nickname.empty()
            ? "Unknown Pokemon"
            : app_.loadService_.pickupSummary.nickname;
        errorDialogLocation_.clear();
        errorDialogMessage_ = result.message.empty()
            ? "The Pokemon payload could not be read."
            : result.message;
        errorDialogVisible_ = true;
        Logger::instance().warning("Cloud swap failed: " + result.message);
        return;
    }
    if (app_.loadService_.pickupCloudBox != static_cast<std::uint16_t>(cloudBox_ + 1)) {
        cachedCloudPayloads_[app_.loadService_.pickupSlot].format = result.pokemon.format;
        cachedCloudPayloads_[app_.loadService_.pickupSlot].data = std::move(result.pokemon.payload);
        app_.status_ = "Box changed, swap cancelled.";
        return;
    }
    PokemonPayload occupantPayload;
    occupantPayload.format = result.pokemon.format;
    occupantPayload.data = std::move(result.pokemon.payload);
    const PokemonSummary occupantSummary = cloudPreview_[app_.loadService_.pickupSlot];
    cloudPreview_[app_.loadService_.pickupSlot] = hand_.summary;
    pendingUploadPayloads_[app_.loadService_.pickupSlot] = hand_.payload;
    hand_.summary = occupantSummary;
    hand_.payload = std::move(occupantPayload);
    hand_.source = HandSource::Cloud;
    hand_.sourceIndex = app_.loadService_.pickupSlot;
    hand_.sourceCloudBox = static_cast<std::uint16_t>(cloudBox_ + 1);
    hand_.payloadKnown = !hand_.payload.data.empty();
    app_.status_ = hand_.summary.nickname + " swapped.";
}

void BankScreen::renderTop(float eyeOffset) {
    drawBoxBackground(app_.resources_.boxBackground, true);

    constexpr float topScale = 400.0F / 320.0F;
    constexpr float nameBarW = 200.0F * topScale;
    constexpr float nameBarY = 6.0F;

    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 8.0F, nameBarY, 0.14F,
                        nullptr, topScale, 1.0F);
    } else {
        drawPill(8.0F, nameBarY, nameBarW, 30.0F, 0.14F, BoxPlateBorder);
        drawPill(10.0F, nameBarY + 1.0F, nameBarW - 4.0F, 27.0F, 0.15F, BoxPlate);
    }
    if (cloudNameFocused_) {
        C2D_DrawRectSolid(8.0F, nameBarY - 1.0F, 0.145F, nameBarW, 2.0F, CursorGreen);
        C2D_DrawRectSolid(8.0F, nameBarY + 25.0F, 0.145F, nameBarW, 2.0F, CursorGreen);
        drawBouncingCursor(8.0F + nameBarW * 0.5F, nameBarY - 6.0F, 3.5F, 12.0F, CursorRed);
    }
    const auto cachedCloudName = cloudBoxNames_.find(static_cast<std::uint16_t>(cloudBox_ + 1));
    const std::string cloudBoxLabel = cachedCloudName != cloudBoxNames_.end()
        ? cachedCloudName->second
        : "Bank " + std::to_string(cloudBox_ + 1);
    app_.drawCentered(cloudBoxLabel, 8.0F + nameBarW * 0.5F, nameBarY + 5.0F, 0.55F, HeaderInk);
    const bool waitingOnHeldPickup = hand_.active && hand_.source == HandSource::Cloud
        && !hand_.payloadKnown
        && app_.loadService_.operation() == LoadService::Operation::PickupCloud;
    if (app_.loadService_.running()
        && ((app_.loadService_.operation() == LoadService::Operation::CloudBox
             && app_.loadService_.cloudBoxKey == static_cast<std::uint16_t>(cloudBox_))
            || waitingOnHeldPickup
            || app_.loadService_.operation() == LoadService::Operation::SwapCloud)) {
        const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
        const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(seconds) * 6.0F);
        C2D_DrawCircleSolid(312.0F, nameBarY + 15.0F, 0.4F, 3.0F + pulse * 2.0F, HeaderInk);
        app_.drawText("Loading", 320.0F, nameBarY + 9.0F, 0.34F, HeaderInk);
    }

    constexpr float pitchX = 34.0F * topScale;
    constexpr float pitchY = 30.0F;
    constexpr float gridLeft = 8.0F;
    constexpr float gridTop = 46.0F;
    drawPlusMark(gridLeft - 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft - 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);

    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float cx = gridLeft + (static_cast<float>(slot % 6) + 0.5F) * pitchX;
        const float cy = gridTop + (static_cast<float>(slot / 6) + 0.5F) * pitchY;
        const PokemonSummary& pokemon = cloudPreview_[slot];
        if (app_.resources_.pokemonSprites && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, pokemon.species);
            constexpr float scale = 1.0F;
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            const float spriteX = std::round(cx - w * 0.5F + eyeOffset * 0.2F);
            const float spriteY = std::round(cy - h * 0.5F);
            const std::uint8_t saveGen = saveAdapter_.gameGeneration();
            const std::uint8_t monFormat = pokemon.format != 0
                ? pokemon.format
                : pokemonFormatFromCode(pokemon.gameCode);
            const bool incompatible = saveGen != 0 && monFormat != 0
                && monFormat > saveGen;
            C2D_ImageTint tint{};
            C2D_PlainImageTint(&tint, C2D_Color32(72, 72, 72, 255), 0.82F);
            C2D_DrawImageAt(image, spriteX, spriteY, 0.3F,
                            incompatible ? &tint : nullptr, scale, scale);
            drawPokemonBadges(app_.resources_.overlayIcons, pokemon, cx, cy, w * 0.5F, h * 0.5F, 0.31F);
        }
        if (storagePane_ == StoragePane::Cloud && slot == focusedSlot_ && !cloudNameFocused_) {
            const u32 arrowColor = hand_.active ? CursorGreen : CursorRed;
            drawBouncingCursor(cx, cy - 22.0F, 3.5F, 12.0F, arrowColor);

            if (hand_.active && hand_.summary.species != 0 && app_.resources_.pokemonSprites) {
                const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, hand_.summary.species);
                constexpr float scale = 1.0F;
                const float w = image.subtex->width * scale;
                const float h = image.subtex->height * scale;
                C2D_DrawImageAt(image,
                                std::round(cx - w * 0.5F),
                                std::round(cy - h * 0.5F),
                                0.6F, nullptr, scale, scale);
            }
        }
    }

    const float gridRight = gridLeft + pitchX * 6.0F;
    const float infoCenterX = gridRight + (400.0F - gridRight) * 0.5F;
    constexpr float infoTop = 8.0F;
    constexpr auto lang = pksm::Language::ENG;

    const PokemonSummary& focused = storagePane_ == StoragePane::Cloud
        ? cloudPreview_[focusedSlot_]
        : (storagePane_ == StoragePane::Party
            ? partyWorking_.summaries[focusedSlot_]
            : storage_.pokemon(focusedSlot_));
    if (focused.species != 0) {
        const std::string speciesName = pksm::Species(focused.species).localize(lang);
        const std::string header = "#" + std::to_string(focused.species) + " "
            + (speciesName.empty() ? focused.nickname : speciesName);
        if (app_.resources_.nameDexPlate) {
            constexpr float plateScaleY = 0.85F;
            const C2D_Image plateImage = C2D_SpriteSheetGetImage(app_.resources_.nameDexPlate, 0);
            const float plateW = 400.0F - gridRight;
            const float plateScaleX = plateW / static_cast<float>(plateImage.subtex->width);
            C2D_DrawImageAt(plateImage, gridRight, infoTop - 4.0F, 0.13F,
                            nullptr, plateScaleX, plateScaleY);
            app_.drawCentered(header, infoCenterX, infoTop, 0.46F,
                              C2D_Color32(250, 247, 238, 255));
        } else {
            app_.drawCentered(header, infoCenterX, infoTop, 0.46F, HeaderInk);
        }
        {
            const C2D_SpriteSheet genderSheet = focused.gender == pksm::Gender::Male
                ? app_.resources_.genderMaleIcon
                : (focused.gender == pksm::Gender::Female
                    ? app_.resources_.genderFemaleIcon
                    : app_.resources_.genderlessIcon);
            if (genderSheet && focused.gender != pksm::Gender::Genderless) {
                const C2D_Image genderImage = C2D_SpriteSheetGetImage(genderSheet, 0);
                if (genderImage.tex) {
                    const float headerW = app_.textWidth(header, 0.46F);
                    constexpr float genderScale = 0.8F;
                    const float iconW = static_cast<float>(genderImage.subtex->width) * genderScale;
                    const float iconH = static_cast<float>(genderImage.subtex->height) * genderScale;
                    const float iconX = std::min(infoCenterX + headerW * 0.5F + 4.0F, 400.0F - 4.0F - iconW);
                    C2D_DrawImageAt(genderImage, iconX, infoTop + 9.0F - iconH * 0.5F,
                                    0.15F, nullptr, genderScale, genderScale);
                }
            }
        }
        static const std::string placeholder = "-----";
        const float colLeft = gridRight;
        const float colWidth = 400.0F - colLeft;
        const float valueRightX = 400.0F - 8.0F;
        int rowIndex = 0;
        const auto drawInfoRow = [&](float y, float height, const std::string& label,
                                      const std::string& value, float fontSize, const C2D_Image* icon) {
            if (rowIndex % 2 == 0 && app_.resources_.infoStripe) {
                const C2D_Image stripeImage = C2D_SpriteSheetGetImage(app_.resources_.infoStripe, 0);
                const float scaleX = colWidth / static_cast<float>(stripeImage.subtex->width);
                const float scaleY = height / static_cast<float>(stripeImage.subtex->height);
                C2D_DrawImageAt(stripeImage, colLeft, y, 0.12F, nullptr, scaleX, scaleY);
            }
            if (app_.resources_.pointSmall) {
                const C2D_Image dotImage = C2D_SpriteSheetGetImage(app_.resources_.pointSmall, 0);
                C2D_DrawImageAt(dotImage, colLeft + 6.0F,
                                y + height * 0.5F - static_cast<float>(dotImage.subtex->height) * 0.5F, 0.13F);
            }
            float textX = colLeft + 16.0F;
            if (icon && icon->tex) {
                const float iconH = static_cast<float>(icon->subtex->height);
                const float iconW = static_cast<float>(icon->subtex->width);
                C2D_DrawImageAt(*icon, textX, y + height * 0.5F - iconH * 0.5F, 0.14F);
                textX += iconW + 4.0F;
            }
            app_.drawText(label, textX, y + 2.0F, fontSize, HeaderInk);
            app_.drawRight(value, valueRightX, y + 2.0F, fontSize, HeaderInk);
            ++rowIndex;
        };

        constexpr float rowH = 15.0F;
        float rowY = infoTop + 18.0F;
        drawInfoRow(rowY, rowH, "Lv.", std::to_string(focused.level), 0.40F, nullptr);
        rowY += rowH;
        drawInfoRow(rowY, rowH, "OT:", focused.trainerName, 0.40F, nullptr);
        rowY += rowH;
        if (!focused.nickname.empty() && focused.nickname != speciesName) {
            drawInfoRow(rowY, rowH, "Nickname:", focused.nickname, 0.40F, nullptr);
            rowY += rowH;
        }
        {
            const std::string itemName = focused.heldItem != 0 ? i18n::item(lang, focused.heldItem) : "";
            drawInfoRow(rowY, rowH, "Item:", itemName.empty() ? placeholder : itemName, 0.38F, nullptr);
            rowY += rowH;
        }
        const std::string abilityName = i18n::ability(lang, focused.ability);
        drawInfoRow(rowY, rowH, "Ability:", abilityName.empty() ? placeholder : abilityName, 0.38F, nullptr);
        rowY += rowH;
        drawInfoRow(rowY, rowH, "Nature:", i18n::nature(lang, focused.nature), 0.38F, nullptr);
        rowY += rowH;
        if (focused.language != pksm::Language::None) {
            drawInfoRow(rowY, rowH, "Lang:", i18n::langString(focused.language), 0.34F, nullptr);
            rowY += rowH;
        }

        const float typeNativeH = typeBannerHeight(app_.resources_.typeBanners, focused.type1);
        constexpr float typeTargetH = 12.0F;
        const float typeScale = typeNativeH > 0.0F ? typeTargetH / typeNativeH : 1.0F;
        const float bannerW1 = typeBannerWidth(app_.resources_.typeBanners, focused.type1, typeScale);
        const float bannerH1 = typeNativeH * typeScale;
        float bannerY = rowY;
        if (rowIndex % 2 == 0 && app_.resources_.infoStripe) {
            const C2D_Image stripeImage = C2D_SpriteSheetGetImage(app_.resources_.infoStripe, 0);
            const float scaleX = colWidth / static_cast<float>(stripeImage.subtex->width);
            const float scaleY = rowH / static_cast<float>(stripeImage.subtex->height);
            C2D_DrawImageAt(stripeImage, colLeft, bannerY, 0.12F, nullptr, scaleX, scaleY);
        }
        ++rowIndex;
        app_.drawText("Type", colLeft + 16.0F, bannerY + 2.0F, 0.40F, HeaderInk);
        const float bannerCenterY = bannerY + (rowH - bannerH1) * 0.5F;
        if (focused.type1 == focused.type2) {
            drawTypeBanner(app_.resources_.typeBanners, focused.type1,
                           valueRightX - bannerW1, bannerCenterY, 0.32F, typeScale);
        } else {
            const float bannerW2 = typeBannerWidth(app_.resources_.typeBanners, focused.type2, typeScale);
            constexpr float bannerGap = 4.0F;
            const float pairLeft = valueRightX - (bannerW1 + bannerGap + bannerW2);
            drawTypeBanner(app_.resources_.typeBanners, focused.type1, pairLeft, bannerCenterY, 0.32F, typeScale);
            drawTypeBanner(app_.resources_.typeBanners, focused.type2,
                           pairLeft + bannerW1 + bannerGap, bannerCenterY, 0.32F, typeScale);
        }
        bannerY += rowH;

        const std::string originName = i18n::game(lang, focused.originGame);
        if (!originName.empty()) {
            drawInfoRow(bannerY, rowH, "Origin:", originName, 0.40F, nullptr);
            bannerY += rowH;
        }

        bannerY += 4.0F;
        for (const pksm::Move& move : focused.moves) {
            const std::string moveName = move == pksm::Move::None ? "" : i18n::move(lang, move);
            drawInfoRow(bannerY, 15.0F, "Move:", moveName.empty() ? placeholder : moveName, 0.38F, nullptr);
            bannerY += 16.0F;
        }
    } else {
        const std::string emptyLabel = storagePane_ == StoragePane::Cloud
            ? "Empty cloud slot"
            : (storagePane_ == StoragePane::Party ? "Empty party slot" : "Empty slot");
        app_.drawCentered(emptyLabel, infoCenterX, infoTop + 16.0F, 0.4F, HeaderInk);
    }
}

void BankScreen::render() {
    if (errorDialogVisible_) {
        renderErrorDialog();
        return;
    }
    renderStorageBottom();
}

void BankScreen::renderStorageBottom() {
    drawLinePattern(app_.resources_.bottomBackground, C2D_Color32(158, 224, 152, 255), false);

    C2D_DrawRectSolid(0.0F, 0.0F, 0.05F, 320.0F, 20.0F, C2D_Color32(215, 232, 224, 235));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.1F, 7.0F, C2D_Color32(210, 40, 40, 255));
    C2D_DrawRectSolid(7.0F, 9.0F, 0.15F, 14.0F, 2.0F, C2D_Color32(30, 30, 30, 255));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.2F, 2.5F, C2D_Color32(240, 240, 240, 255));
    const bool fetching = app_.loadService_.running()
        && ((hand_.active && hand_.source == HandSource::Cloud && !hand_.payloadKnown
             && app_.loadService_.operation() == LoadService::Operation::PickupCloud)
            || app_.loadService_.operation() == LoadService::Operation::SwapCloud);
    app_.drawText(fetching ? "FETCHING" : (hand_.active ? "HOLDING" : "READY"),
             30.0F, 6.0F, 0.34F, fetching ? CursorGreen : (hand_.active ? CursorGreen : HeaderInk));
    if (hasPendingChanges()) {
        app_.drawText("PENDING", 100.0F, 6.0F, 0.34F, CursorGreen);
    }
    C2D_DrawRectSolid(252.0F, 2.0F, 0.1F, 60.0F, 16.0F, C2D_Color32(58, 58, 58, 255));
    app_.drawCentered("START", 282.0F, 5.0F, 0.4F, C2D_Color32(240, 240, 240, 255));

    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 6.0F, 26.0F, 0.14F);
    } else {
        drawPill(6.0F, 26.0F, 200.0F, 26.0F, 0.14F, BoxPlateBorder);
        drawPill(8.0F, 27.0F, 196.0F, 23.0F, 0.15F, BoxPlate);
    }
    const std::string boxLabel = localBoxName_.empty()
        ? "BOX " + std::to_string(localBox_ + 1)
        : localBoxName_;
    app_.drawCentered(boxLabel, 106.0F, 31.0F, 0.55F, HeaderInk);

    constexpr float pitchX = 34.0F;
    constexpr float pitchY = 30.0F;
    constexpr float gridLeft = 8.0F;
    constexpr float gridTop = 58.0F;
    drawPlusMark(gridLeft - 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft - 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);

    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float cx = gridLeft + (static_cast<float>(slot % 6) + 0.5F) * pitchX;
        const float cy = gridTop + (static_cast<float>(slot / 6) + 0.5F) * pitchY;
        const PokemonSummary& pokemon = storage_.pokemon(slot);
        const bool isSelected = storage_.selected(slot);
        if (app_.resources_.pokemonSprites && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, pokemon.species);
            constexpr float scale = 1.0F;
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            C2D_DrawImageAt(image, std::round(cx - w * 0.5F), std::round(cy - h * 0.5F),
                            0.3F, nullptr, scale, scale);
            if (isSelected) {
                const float half = 15.0F;
                C2D_DrawRectSolid(cx - half, cy - half, 0.35F, half * 2.0F, half * 2.0F,
                                  C2D_Color32(255, 255, 255, 160));
            }
            drawPokemonBadges(app_.resources_.overlayIcons, pokemon, cx, cy, w * 0.5F, h * 0.5F, 0.36F);
        }
        if (storagePane_ == StoragePane::Local && slot == focusedSlot_) {
            const u32 arrowColor = hand_.active ? CursorGreen : CursorRed;
            drawBouncingCursor(cx, cy - 20.0F, 3.0F, 10.0F, arrowColor);

            if (hand_.active && hand_.summary.species != 0 && app_.resources_.pokemonSprites) {
                const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, hand_.summary.species);
                constexpr float scale = 1.0F;
                const float w = image.subtex->width * scale;
                const float h = image.subtex->height * scale;
                C2D_DrawImageAt(image,
                                std::round(cx - w * 0.5F),
                                std::round(cy - h * 0.5F),
                                0.6F, nullptr, scale, scale);
            }
        }
    }

    constexpr float teamHeaderX = 218.0F;
    constexpr float teamHeaderY = 26.0F;
    constexpr float teamHeaderW = 96.0F;
    constexpr float teamHeaderH = 26.0F;
    constexpr float teamHeaderBgW = 320.0F - teamHeaderX;
    if (app_.resources_.teamBackground) {
        const C2D_Image headerBg = C2D_SpriteSheetGetImage(app_.resources_.teamBackground, 0);
        if (headerBg.subtex) {
            const float scaleX = teamHeaderBgW / static_cast<float>(headerBg.subtex->width);
            const float scaleY = teamHeaderH / static_cast<float>(headerBg.subtex->height);
            C2D_DrawImageAt(headerBg, teamHeaderX, teamHeaderY, 0.14F, nullptr, scaleX, scaleY);
        }
    } else {
        drawPill(teamHeaderX, teamHeaderY, teamHeaderW, teamHeaderH, 0.14F, BoxPlateBorder);
        drawPill(teamHeaderX + 2.0F, teamHeaderY + 1.0F, teamHeaderW - 4.0F, teamHeaderH - 3.0F, 0.15F, BoxPlate);
    }
    app_.drawCentered("TEAM", teamHeaderX + teamHeaderW * 0.5F, 31.0F, 0.55F,
                      app_.resources_.teamBackground ? C2D_Color32(255, 255, 255, 255) : HeaderInk);

    constexpr float partyColAX = 244.0F;
    constexpr float partyColBX = 288.0F;
    constexpr float partyRowStep = 45.0F;
    constexpr float partyColATop = 86.0F;
    constexpr float partyColBTop = 108.0F;
    const int partyCount = partyMemberCount();
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const std::size_t column = slot % 2;
        const std::size_t row = slot / 2;
        const float cx = column == 0 ? partyColAX : partyColBX;
        const float cy = (column == 0 ? partyColATop : partyColBTop) + static_cast<float>(row) * partyRowStep;
        constexpr float tileSize = 34.0F;
        drawRoundedRect(cx - tileSize * 0.5F, cy - tileSize * 0.5F, tileSize, tileSize, 8.0F, 0.10F, CursorGreen);
        drawRoundedRect(cx - tileSize * 0.5F + 2.0F, cy - tileSize * 0.5F + 2.0F, tileSize - 4.0F, tileSize - 4.0F,
                        7.0F, 0.11F, BoxPlate);
        const PokemonSummary& pokemon = partyWorking_.summaries[slot];

        const bool isLastMember = pokemon.species != 0 && partyCount <= 1;
        if (app_.resources_.pokemonSprites && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, pokemon.species);
            constexpr float scale = 1.0F;
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            C2D_ImageTint lockedTint{};
            C2D_PlainImageTint(&lockedTint, C2D_Color32(72, 72, 72, 255), 0.82F);
            C2D_DrawImageAt(image, std::round(cx - w * 0.5F), std::round(cy - h * 0.5F),
                            0.3F, isLastMember ? &lockedTint : nullptr, scale, scale);
            drawPokemonBadges(app_.resources_.overlayIcons, pokemon, cx, cy, w * 0.5F, h * 0.5F, 0.36F);
        }
        if (storagePane_ == StoragePane::Party && slot == focusedSlot_) {
            const u32 arrowColor = hand_.active ? CursorGreen : CursorRed;
            drawBouncingCursor(cx, cy - 20.0F, 3.0F, 10.0F, arrowColor);
            if (hand_.active && hand_.summary.species != 0 && app_.resources_.pokemonSprites) {
                const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, hand_.summary.species);
                constexpr float scale = 1.0F;
                const float w = image.subtex->width * scale;
                const float h = image.subtex->height * scale;
                C2D_DrawImageAt(image,
                                std::round(cx - w * 0.5F),
                                std::round(cy - h * 0.5F),
                                0.6F, nullptr, scale, scale);
            }
        }
    }

    if (commitJob_.running()) {
        C2D_DrawRectSolid(0.0F, 205.0F, 0.7F, 200.0F, 35.0F, C2D_Color32(0, 0, 0, 170));
        const int phase = commitPhase_.load(std::memory_order_acquire);
        std::string label = phase == 3 ? "Writing save..."
                           : phase == 2 ? "Uploading cloud..."
                           : phase == 1 ? "Removing cloud..."
                           : "Preparing...";
        const int progress = commitProgress_.load(std::memory_order_acquire);
        label += " " + std::to_string(progress) + "%";
        app_.drawCentered(label,
                     100.0F, 210.0F, 0.42F, C2D_Color32(255, 255, 255, 255));
        C2D_DrawRectSolid(10.0F, 228.0F, 0.9F, 180.0F, 6.0F, C2D_Color32(50, 50, 50, 220));
        const float fill = 180.0F * static_cast<float>(progress) / 100.0F;
        C2D_DrawRectSolid(10.0F, 228.0F, 0.92F, fill, 6.0F, CursorGreen);
        return;
    }
    const bool held = hand_.active;
    const bool pending = hasPendingChanges();

    C2D_DrawRectSolid(0.0F, 210.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, 25));
    C2D_DrawRectSolid(0.0F, 216.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, 45));
    C2D_DrawRectSolid(0.0F, 222.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, 65));
    C2D_DrawRectSolid(0.0F, 228.0F, 0.06F, 320.0F, 12.0F, C2D_Color32(255, 255, 255, 85));

    const u32 divider = C2D_Color32(20, 110, 70, 55);
    C2D_DrawRectSolid(72.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);
    C2D_DrawRectSolid(152.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);

    const u32 aGlyph = held ? CursorGreen : C2D_Color32(216, 40, 32, 150);
    app_.drawCentered("A", 16.0F, 219.0F, 0.42F, aGlyph);
    app_.drawText(held ? "Drop" : "Pick", 26.0F, 220.0F, 0.4F, HeaderInk);

    const u32 bGlyph = held ? C2D_Color32(120, 60, 160, 220) : C2D_Color32(20, 110, 70, 140);
    app_.drawCentered("B", 82.0F, 219.0F, 0.42F, bGlyph);
    app_.drawText(held ? "Return" : "Back", 92.0F, 220.0F, 0.4F, HeaderInk);

    const u32 selGlyph = pending ? CursorGreen : C2D_Color32(20, 110, 70, 140);
    app_.drawCentered("S", 162.0F, 219.0F, 0.42F, selGlyph);
    app_.drawText("Save", 172.0F, 220.0F, 0.4F, pending ? CursorGreen : HeaderInk);
}

void BankScreen::renderErrorDialog() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, C2D_Color32(12, 24, 19, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.40F, 284.0F, 208.0F, C2D_Color32(250, 247, 238, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.45F, 7.0F, 208.0F, Error);
    C2D_DrawRectSolid(25.0F, 20.0F, 0.45F, 277.0F, 36.0F, C2D_Color32(255, 225, 214, 255));

    app_.drawText(errorDialogTitle_, 36.0F, 30.0F, 0.52F, Error);
    app_.drawText(errorDialogPokemon_.empty() ? "Unknown Pokemon" : errorDialogPokemon_,
             36.0F, 68.0F, 0.62F, HeaderInk);
    if (!errorDialogLocation_.empty()) {
        app_.drawText(errorDialogLocation_, 36.0F, 90.0F, 0.38F, Muted);
    }

    std::vector<std::string> lines;
    std::string remaining = errorDialogMessage_.empty()
        ? "The transfer was rejected. Check rebank.log for details."
        : errorDialogMessage_;
    constexpr std::size_t MaxLineLength = 42;
    while (!remaining.empty() && lines.size() < 4) {
        if (remaining.size() <= MaxLineLength) {
            lines.push_back(remaining);
            break;
        }
        std::size_t split = remaining.rfind(' ', MaxLineLength);
        if (split == std::string::npos || split == 0) {
            split = MaxLineLength;
        }
        lines.push_back(remaining.substr(0, split));
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ') {
            remaining.erase(remaining.begin());
        }
    }
    if (!remaining.empty() && !lines.empty()) {
        std::string& last = lines.back();
        if (last.size() > MaxLineLength - 3) {
            last.resize(MaxLineLength - 3);
        }
        last += "...";
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        app_.drawText(lines[index], 36.0F, 116.0F + static_cast<float>(index) * 15.0F,
                 0.36F, HeaderInk);
    }

    const UiRect okButton{92.0F, 190.0F, 136.0F, 34.0F};
    C2D_DrawRectSolid(okButton.x, okButton.y, 0.46F, okButton.width, okButton.height, Brand);
    C2D_DrawRectSolid(okButton.x + 2.0F, okButton.y + 2.0F, 0.47F,
                      okButton.width - 4.0F, okButton.height - 4.0F, CursorGreen);
    app_.drawCentered("OK", 160.0F, 199.0F, 0.52F, C2D_Color32(255, 255, 255, 255));
    app_.drawCentered("A / B", 268.0F, 201.0F, 0.30F, Muted);
}

void BankScreen::reset() {
    saveAdapter_.close();
    saveSummary_ = {};
    cloudBoxes_.clear();
    cloudPrefetchCooldownUntil_.clear();
    localBaselines_.clear();
    localDrafts_.clear();
    cloudPreview_.fill({});
    partyBaseline_ = PartyDraft{};
    partyWorking_ = PartyDraft{};
}
