#pragma once

#include "bank/commit/CommitPlan.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class RemoteOutcome : std::uint8_t {
    Done,
    Refused,
    Unknown
};

struct RemoteResult {
    RemoteOutcome outcome = RemoteOutcome::Unknown;
    std::string message;
};

struct CloudRejection {
    SlotRef slot;
    std::string reason;
};

struct UploadBatchResult {
    RemoteOutcome outcome = RemoteOutcome::Unknown;
    std::string message;
    std::vector<CloudRejection> rejected;
};

enum class CloudClaim : std::uint8_t {
    Claimed,
    Changed,
    Locked
};

struct ClaimBatchResult {
    RemoteOutcome outcome = RemoteOutcome::Unknown;
    std::string message;
    std::vector<CloudClaim> claims;
    std::vector<std::optional<PokemonPayload>> received;
};

struct ClaimItem {
    SlotRef slot;
    PokemonPayload payload;
};

struct UploadItem {
    SlotRef destination;
    PokemonSummary summary;
    PokemonPayload payload;
    bool replaceOccupant = false;
};

class CommitBackend {
public:
    virtual ~CommitBackend() = default;
    virtual RemoteResult moveCloud(const SlotRef& from, const SlotRef& to) = 0;
    virtual UploadBatchResult uploadBatch(const std::vector<UploadItem>& items) = 0;
    virtual std::optional<std::array<PokemonPayload, BoxSlotCount>> readCloudBox(std::uint16_t boxKey) = 0;
    virtual RemoteResult deleteCloud(const std::vector<SlotRef>& slots) = 0;
    virtual ClaimBatchResult claimCloud(const std::vector<ClaimItem>& items) = 0;
    virtual bool writeLocal(const SlotRef& slot, const PokemonPayload& payload) = 0;
    virtual bool clearLocal(const SlotRef& slot) = 0;
    virtual bool persistLocal(std::string& error, bool finalWrite) = 0;
    virtual void discardLocal() = 0;
    virtual void log(const std::string&) {}
};
