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

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace Gui;

void BankScreen::update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    if (commitJob_.running()
        || (app_.loadService_.running()
            && app_.loadService_.operation() == LoadService::Operation::CloudBox)) {
        return;
    }
    if (keysDown & KEY_B) {
        if (hand_.active) {
            storageReturnHand();
            return;
        }
        if (hasPendingChanges()) {
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
        std::size_t column = focusedSlot_ % 6;
        std::size_t row = focusedSlot_ / 6;
        if (direction == 1) {
            row = row == 0 ? 0 : row - 1;
        } else if (direction == 2) {
            row = row == 4 ? 4 : row + 1;
        } else if (direction == 3) {
            column = column == 0 ? 5 : column - 1;
        } else if (direction == 4) {
            column = (column + 1) % 6;
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
    }

    if (keysDown & KEY_SELECT) {
        if (hand_.active) {
            app_.status_ = "Drop the Pokemon first.";
        } else if (app_.loadService_.running()) {
            app_.status_ = "Please wait for the current fetch to finish.";
        } else if (!hasPendingChanges()) {
            app_.status_ = "Nothing to commit.";
        } else {
            beginCommit();
        }
    }

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
    } else {
        cloudPreview_[hand_.sourceIndex] = hand_.summary;
        if (!hand_.payload.data.empty()) {
            cachedCloudPayloads_[hand_.sourceIndex] = hand_.payload;
        }
    }
    app_.status_ = "Returned to slot " + std::to_string(hand_.sourceIndex + 1) + ".";
    hand_ = Hand{};
}

bool BankScreen::hasPendingChanges() const {
    for (const auto& [box, draft] : localDrafts_) {
        auto it = localBaselines_.find(box);
        if (it == localBaselines_.end()) {
            return true;
        }
        for (std::size_t slot = 0; slot < 30; ++slot) {
            if (draft.summaries[slot].species != it->second.summaries[slot].species
                || draft.summaries[slot].nickname != it->second.summaries[slot].nickname
                || draft.payloads[slot].data != it->second.payloads[slot].data) {
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
                return true;
            }
        }
    }
    for (std::size_t slot = 0; slot < 30; ++slot) {
        if (!pendingUploadPayloads_[slot].data.empty()) {
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
        baseline.summaries = saveAdapter_.readBox(localBox_);
        for (std::size_t slot = 0; slot < 30; ++slot) {
            baseline.payloads[slot] = saveAdapter_.readPokemon(localBox_, slot);
        }
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
        app_.status_.clear();
        app_.loadService_.cloudBoxKey = boxKey;
        app_.loadService_.begin(LoadService::Operation::CloudBox);
        return;
    }
    cloudPreview_ = it->second.summaries;
    pendingUploadPayloads_ = it->second.pending;
    cachedCloudPayloads_ = {};
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
    cloudBoxes_.clear();
    pendingUploadPayloads_ = {};
    cachedCloudPayloads_ = {};
    loadLocalBox();
    storagePane_ = previousPane;
    refreshCloudBox();
    app_.status_ = "Pending changes discarded.";
    Logger::instance().info("Pending storage changes discarded");
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
    const std::size_t uploadBatchCount = (uploads.size() + 29) / 30;
    const std::size_t totalSteps = localChangeCount + (localChangeCount > 0 ? 1 : 0)
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
        localBaselines_.clear();
        localDrafts_.clear();
        cloudBoxes_.clear();
        loadLocalBox();
        refreshCloudBox(true);
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
    cloudBoxes_.clear();
    LocalBoxDraft baseline;
    baseline.summaries = std::move(result.localPokemon);
    baseline.payloads = std::move(result.localPayloads);
    localBaselines_[localBox_] = std::move(baseline);
    storage_.load(localBaselines_[localBox_].summaries);
    localPayloads_ = localBaselines_[localBox_].payloads;
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
    CloudBoxDraft cloud;
    if (app_.cloudBoxCache_.success) {
        cloud.baseline = app_.cloudBoxCache_.pokemon;
        cloud.summaries = app_.cloudBoxCache_.pokemon;
        cloudBoxes_[0] = cloud;
        cloudPreview_ = cloud.summaries;
        app_.status_.clear();
    } else {
        cloudPreview_.fill({});
        app_.status_ = app_.cloudBoxCache_.message;
    }
    pendingUploadPayloads_ = {};
    cachedCloudPayloads_ = {};
    hand_ = Hand{};
    app_.screen_ = App::Screen::Bank;
}

void BankScreen::onCloudBoxLoaded() {
    const auto boxKey = app_.loadService_.cloudBoxKey;
    const BoxListResult& result = app_.loadService_.cloudBoxResult;
    if (result.success) {
        CloudBoxDraft draft;
        draft.baseline = result.pokemon;
        draft.summaries = result.pokemon;
        cloudBoxes_[boxKey] = std::move(draft);
        if (cloudBox_ == boxKey) {
            cloudPreview_ = cloudBoxes_[boxKey].summaries;
            pendingUploadPayloads_ = cloudBoxes_[boxKey].pending;
            cachedCloudPayloads_ = {};
        }
        app_.status_.clear();
        return;
    }
    if (cloudBox_ == boxKey) {
        cloudPreview_.fill({});
        pendingUploadPayloads_ = {};
    }
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
    if (stillHeld) {
        cloudPreview_[app_.loadService_.pickupSlot] = hand_.summary;
        hand_ = Hand{};
    }
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

    drawPill(8.0F, 6.0F, 320.0F, 28.0F, 0.1F, HeaderPill);
    C2D_DrawRectSolid(330.0F, 6.0F, 0.1F, 62.0F, 28.0F, CountBlock);
    for (int i = 0; i < 4; ++i) {
        C2D_DrawRectSolid(332.0F + i * 15.0F, 8.0F, 0.11F, 3.0F, 24.0F,
                          C2D_Color32(210, 24, 24, 255));
    }
    app_.drawText("Group: ReBank", 22.0F, 12.0F, 0.55F, HeaderInk);
    app_.drawCentered(std::to_string(storage_.selectedCount()), 361.0F, 12.0F, 0.60F,
                 C2D_Color32(255, 255, 255, 255));

    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 100.0F, 44.0F, 0.14F);
    } else {
        drawPill(60.0F, 44.0F, 280.0F, 30.0F, 0.14F, BoxPlateBorder);
        drawPill(62.0F, 45.0F, 276.0F, 27.0F, 0.15F, BoxPlate);
    }
    if (cloudNameFocused_) {
        C2D_DrawRectSolid(100.0F, 43.0F, 0.145F, 200.0F, 2.0F, CursorGreen);
        C2D_DrawRectSolid(100.0F, 69.0F, 0.145F, 200.0F, 2.0F, CursorGreen);
        drawBouncingCursor(200.0F, 30.0F, 3.5F, 12.0F, CursorRed);
    }
    const auto cachedCloudName = cloudBoxNames_.find(static_cast<std::uint16_t>(cloudBox_ + 1));
    const std::string cloudBoxLabel = cachedCloudName != cloudBoxNames_.end()
        ? cachedCloudName->second
        : "Bank " + std::to_string(cloudBox_ + 1);
    const float cloudNameY = 57.0F - textHeight(app_.resources_.textFont, app_.resources_.textBuffer, cloudBoxLabel, 0.55F) * 0.5F;
    app_.drawCentered(cloudBoxLabel, 200.0F, cloudNameY, 0.55F, HeaderInk);
    if (app_.loadService_.running()
        && (app_.loadService_.operation() == LoadService::Operation::CloudBox
            || app_.loadService_.operation() == LoadService::Operation::PickupCloud
            || app_.loadService_.operation() == LoadService::Operation::SwapCloud)) {
        const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
        const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(seconds) * 6.0F);
        C2D_DrawCircleSolid(312.0F, 59.0F, 0.4F, 3.0F + pulse * 2.0F, HeaderInk);
        app_.drawText("Loading", 320.0F, 53.0F, 0.34F, HeaderInk);
    }

    constexpr float pitchX = 55.0F;
    constexpr float pitchY = 30.0F;
    constexpr float gridLeft = 400.0F * 0.5F - pitchX * 3.0F;
    constexpr float gridTop = 84.0F;
    drawPlusMark(gridLeft - 6.0F, gridTop - 4.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 6.0F, gridTop - 4.0F, HeaderInk);
    drawPlusMark(gridLeft - 6.0F, gridTop + pitchY * 5.0F + 4.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 6.0F, gridTop + pitchY * 5.0F + 4.0F, HeaderInk);

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
            // Greyed out when the mon is newer than the save: it can never move
            // down a generation. Pure integer compare, so no per-frame parsing.
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
        && (app_.loadService_.operation() == LoadService::Operation::PickupCloud
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
    const float localNameY = 39.0F - textHeight(app_.resources_.textFont, app_.resources_.textBuffer, boxLabel, 0.55F) * 0.5F;
    app_.drawCentered(boxLabel, 106.0F, localNameY, 0.55F, HeaderInk);

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

    constexpr float sidebarX = 220.0F;
    const PokemonSummary& focused = storagePane_ == StoragePane::Cloud
        ? cloudPreview_[focusedSlot_]
        : storage_.pokemon(focusedSlot_);
    if (focused.species != 0) {
        app_.drawText(focused.nickname, sidebarX + 4.0F, 26.0F, 0.58F, HeaderInk);
        app_.drawText("Lv. " + std::to_string(focused.level), sidebarX + 12.0F, 48.0F, 0.5F, SidebarInk);
        for (int i = 0; i < 6; ++i) {
            C2D_DrawCircleSolid(sidebarX + 8.0F + i * 14.0F, 74.0F, 0.2F, 2.5F, SidebarInk);
        }
        app_.drawText(focused.nickname, sidebarX + 4.0F, 86.0F, 0.44F, SidebarInk);
        drawPill(sidebarX + 4.0F, 106.0F, 84.0F, 18.0F, 0.22F, TypeElectric);
        app_.drawCentered("TYPE", sidebarX + 46.0F, 110.0F, 0.42F, C2D_Color32(255, 255, 255, 255));
        app_.drawText("DEX NO.", sidebarX + 4.0F, 132.0F, 0.44F, SidebarInk);
        app_.drawText(std::to_string(focused.species), sidebarX + 32.0F, 152.0F, 0.62F, SidebarInk);
        app_.drawText(focused.trainerName, sidebarX + 4.0F, 180.0F, 0.44F, SidebarInk);
        app_.drawText("ID. " + std::to_string(saveSummary_.trainerId % 100000),
                 sidebarX + 4.0F, 198.0F, 0.44F, SidebarInk);
    } else {
        app_.drawText(storagePane_ == StoragePane::Cloud ? "Empty cloud slot" : "Empty slot",
                 sidebarX + 4.0F, 110.0F, 0.42F, HeaderInk);
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
    const bool cloudPane = storagePane_ == StoragePane::Cloud;
    const bool held = hand_.active;
    const bool pending = hasPendingChanges();

    const u32 aCircle = held ? CursorGreen : C2D_Color32(200, 40, 40, 255);
    C2D_DrawCircleSolid(20.0F, 220.0F, 0.2F, 10.0F, aCircle);
    app_.drawCentered("A", 20.0F, 214.0F, 0.55F, C2D_Color32(255, 255, 255, 255));
    app_.drawText(held ? "DROP" : "PICK", 36.0F, 214.0F, 0.5F, HeaderInk);

    const u32 bCircle = held ? C2D_Color32(120, 60, 160, 255)
                             : C2D_Color32(90, 90, 90, 255);
    C2D_DrawCircleSolid(96.0F, 220.0F, 0.2F, 10.0F, bCircle);
    app_.drawCentered("B", 96.0F, 214.0F, 0.55F, C2D_Color32(255, 255, 255, 255));
    app_.drawText(held ? "RETURN" : "BACK", 112.0F, 214.0F, 0.5F, HeaderInk);

    const u32 selCircle = pending ? CursorGreen : C2D_Color32(90, 90, 90, 255);
    C2D_DrawCircleSolid(190.0F, 220.0F, 0.2F, 10.0F, selCircle);
    app_.drawCentered("SEL", 190.0F, 216.0F, 0.36F, C2D_Color32(255, 255, 255, 255));
    app_.drawText(pending ? "COMMIT" : (cloudPane ? "LOCAL" : "CLOUD"),
             205.0F, 214.0F, 0.5F, pending ? CursorGreen : HeaderInk);
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
    localBaselines_.clear();
    localDrafts_.clear();
    cloudPreview_.fill({});
}
