#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/CommitService.hpp"
#include "bank/LoadService.hpp"
#include "bank/StorageController.hpp"
#include "core/AsyncTask.hpp"
#include "selection/SelectionController.hpp"

#include <3ds.h>

#include <cstdint>
#include <optional>
#include <string>

class CloudSyncController {
public:
    CloudSyncController(BankContext& context, BankSession& session, StorageController& storage,
                        CommitService& commit, SelectionController& selection)
        : context_(context), session_(session), storage_(storage), commit_(commit), selection_(selection) {}

    void pumpBackgroundFetches();

    void onCloudBoxLoaded();
    void onCloudPickupCompleted();
    void onCloudSwapCompleted();

    void beginRenameBox(std::uint16_t position, std::string name);
    void pollRenameBox();
    bool renameInProgress() const { return rename_.running(); }

private:
    struct RenameJob {
        std::uint16_t position = 0;
        RenameBoxResult result;
    };

    void pumpHandPayloadFetch();
    void pumpHeldRegionPayloadFetch();
    void pumpCloudPayloadPrefetch();
    void pumpCloudPrefetch();
    std::optional<std::uint16_t> nextCloudPrefetchKey() const;
    bool blockedByOtherWork() const;
    void fetchPayload(std::size_t slot, std::uint16_t cloudPosition, const PokemonSummary& summary);
    void rememberCloudPayload(std::uint16_t cloudPosition, std::size_t slot, const PokemonPayload& payload);
    void showFetchError(TextId title, const std::string& message);

    BankContext& context_;
    BankSession& session_;
    StorageController& storage_;
    CommitService& commit_;
    SelectionController& selection_;
    AsyncTask<RenameJob> rename_;
    u64 regionFetchRetryAt_ = 0;
};
