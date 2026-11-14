#pragma once

#include "bank/BankContext.hpp"
#include "bank/BankSession.hpp"
#include "bank/LoadService.hpp"
#include "bank/StorageController.hpp"

#include <cstddef>

class HandController {
public:
    HandController(BankContext& context, BankSession& session, StorageController& storage)
        : context_(context), session_(session), storage_(storage) {}

    void pickUp();
    void drop();
    void pickUpOrDrop() { session_.hand.active ? drop() : pickUp(); }
    void completeCloudSwap(std::size_t slot, PokemonPayload occupantPayload);
    void returnHand();

private:
    void placeHand(StorageAddress address, std::size_t slot);
    void swapHand(StorageAddress address, std::size_t slot, SlotContents occupant);
    void startCloudFetch(LoadService::Operation operation, std::size_t slot, const PokemonSummary& summary);

    BankContext& context_;
    BankSession& session_;
    StorageController& storage_;
};
