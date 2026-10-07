#pragma once

#include "bank/CommitExecutor.hpp"
#include "network/ApiClient.hpp"
#include "save/adapter/SaveAdapter.hpp"

#include <string>
#include <vector>

class DeviceCommitBackend final : public CommitBackend {
public:
    DeviceCommitBackend(ApiClient& api, SaveAdapter& save, std::string accessToken)
        : api_(api), save_(save), accessToken_(std::move(accessToken)) {}

    RemoteResult moveCloud(const SlotRef& from, const SlotRef& to) override;
    UploadBatchResult uploadBatch(const std::vector<UploadItem>& items) override;
    std::optional<std::array<PokemonPayload, BoxSlotCount>> readCloudBox(std::uint16_t boxKey) override;
    RemoteResult deleteCloud(const std::vector<SlotRef>& slots) override;
    ClaimBatchResult claimCloud(const std::vector<ClaimItem>& items) override;
    bool writeLocal(const SlotRef& slot, const PokemonPayload& payload) override;
    bool clearLocal(const SlotRef& slot) override;
    bool persistLocal(std::string& error, bool finalWrite) override;
    void discardLocal() override;
    void log(const std::string& message) override;

private:
    ApiClient& api_;
    SaveAdapter& save_;
    std::string accessToken_;
};
