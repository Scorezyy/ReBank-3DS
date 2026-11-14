#pragma once

#include "bank/BankTypes.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class SlotKind : std::uint8_t {
    Local,
    Party,
    Cloud
};

struct SlotRef {
    SlotKind kind = SlotKind::Local;
    std::uint16_t box = 0;
    std::uint8_t slot = 0;

    static SlotRef local(std::size_t box, std::size_t slot) {
        return {SlotKind::Local, static_cast<std::uint16_t>(box), static_cast<std::uint8_t>(slot)};
    }
    static SlotRef party(std::size_t slot) {
        return {SlotKind::Party, 0, static_cast<std::uint8_t>(slot)};
    }
    static SlotRef cloud(std::uint16_t boxKey, std::size_t slot) {
        return {SlotKind::Cloud, boxKey, static_cast<std::uint8_t>(slot)};
    }

    bool isCloud() const { return kind == SlotKind::Cloud; }
    std::uint16_t cloudPosition() const { return static_cast<std::uint16_t>(box + 1); }
    std::uint8_t oneBasedSlot() const { return static_cast<std::uint8_t>(slot + 1); }

    friend bool operator==(const SlotRef&, const SlotRef&) = default;
    friend auto operator<=>(const SlotRef&, const SlotRef&) = default;
};

std::string describeSlot(const SlotRef& ref);
std::string describePokemon(const PokemonSummary& summary);

enum class IssueReason : std::uint8_t {
    DataNotLoaded,
    OriginUnconfirmed,
    NotInTrash,
    LinkedToHeldBack,
    NoParkingSlot,
    CloudMoveFailed,
    WriteFailed,
    RemoveFailed,
    CloudRefused,
    UploadUnconfirmed,
    UploadNotStored,
    CloudSlotDifferent,
    UploadUndone,
    UploadNotUndone,
    SwapCancelled,
    PlacedElsewhere,
    ReturnedToOrigin,
    OverwriteUnconfirmed,
    DownloadNotRemoved,
    CloudRemoveFailed,
    CloudSlotTrading,
    CloudClaimFailed
};

const char* describeReason(IssueReason reason);

struct CommitIssue {
    std::string pokemon;
    SlotRef location;
    IssueReason reason = IssueReason::LinkedToHeldBack;
    std::string detail;
};

enum class CommitStop : std::uint8_t {
    None,
    CloudMoveFailed,
    SaveFailed
};

struct CommitResult {
    bool success = false;
    CommitStop stop = CommitStop::None;
    std::string stopDetail;
    std::size_t uploads = 0;
    std::size_t downloads = 0;
    std::size_t moves = 0;
    std::size_t deletes = 0;
    std::vector<CommitIssue> issues;
};

struct CommitSnapshot {
    std::unordered_map<std::size_t, BoxSlots> localBaselines;
    std::unordered_map<std::size_t, BoxSlots> localDrafts;
    PartySlots partyBaseline;
    PartySlots partyWorking;
    std::unordered_map<std::uint16_t, CloudBoxDraft> cloudBoxes;
    std::vector<PokemonPayload> confirmedDeletions;
};

enum class TransferKind : std::uint8_t {
    LocalMove,
    Upload,
    CloudMove,
    Download,
    LocalDelete,
    CloudDelete
};

struct PlannedTransfer {
    TransferKind kind = TransferKind::LocalMove;
    SlotRef origin;
    SlotRef destination;
    PokemonSummary summary;
    PokemonPayload payload;
    std::optional<SlotRef> staging;
    std::optional<SlotRef> cloudSlotAfterMoves;
    std::optional<std::size_t> displaced;
    std::size_t group = 0;

    bool landsLocally() const { return kind == TransferKind::LocalMove || kind == TransferKind::Download; }
};

struct CloudMoveStep {
    SlotRef from;
    SlotRef to;
};

struct HeldBackPokemon {
    PokemonSummary summary;
    SlotRef location;
    IssueReason reason = IssueReason::LinkedToHeldBack;
};

using LocalLayout = std::map<SlotRef, std::optional<std::size_t>>;

struct CommitPlan {
    std::vector<PlannedTransfer> transfers;
    std::vector<CloudMoveStep> cloudMoves;
    std::vector<HeldBackPokemon> heldBack;
    LocalLayout baselineLayout;
    LocalLayout stagedLayout;

    bool empty() const { return transfers.empty(); }
};

struct LocalSlotProbe {
    std::size_t boxCount = 0;
    std::function<bool(std::size_t box, std::size_t slot)> occupied;
    std::size_t slotsPerBox = BoxSlotCount;
};

CommitPlan planCommit(const CommitSnapshot& snapshot, const LocalSlotProbe& probe);
