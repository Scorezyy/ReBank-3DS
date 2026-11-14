#include "bank/HandController.hpp"

#include "platform/Logger.hpp"
#include "security/PayloadHash.hpp"

#include <utility>

namespace {
void logSlot(const std::string& action, const std::string& location, const PokemonSummary& mon,
             const PokemonPayload& payload) {
    Logger::instance().info(action + ": " + location + " species " + std::to_string(mon.species)
                            + " \"" + mon.nickname + "\" payload=" + payloadTag(payload.data)
                            + " (" + std::to_string(payload.data.size()) + " bytes)");
}
}

void HandController::pickUp() {
    if (session_.hand.active) {
        return;
    }
    const StorageAddress address = session_.focusedAddress();
    const std::size_t slot = session_.focusedSlot;
    SlotContents contents;
    if (storage_.slots().read(address, slot, contents) == SlotState::Empty) {
        context_.status = context_.text.get(TextId::SlotEmpty);
        return;
    }
    if (address.pane == StoragePane::Party && session_.party.occupiedCount() <= 1) {
        context_.status = context_.text.get(TextId::TeamCannotBeEmpty);
        return;
    }
    if (address.isCloudBank() && !context_.signedIn()) {
        context_.status = context_.text.get(TextId::SignInAgain);
        return;
    }
    storage_.slots().clear(address, slot);
    Hand& hand = session_.hand;
    hand.take(storage_.slots().held(address, slot, std::move(contents)));
    ++session_.handGeneration;
    context_.status = context_.text.format(TextId::PokemonPickedUp, {hand.summary.nickname});
    logSlot(hand.payloadKnown ? "pickUp" : "pickUp (fetching)", storage_.slots().describe(address, slot), hand.summary,
            hand.payload);
    if (!hand.payloadKnown) {
        startCloudFetch(LoadService::Operation::PickupCloud, slot, hand.summary);
    }
}


void HandController::drop() {
    const Hand& hand = session_.hand;
    if (!hand.active) {
        return;
    }
    if (!hand.payloadKnown) {
        context_.status = context_.text.format(TextId::StillFetching, {hand.summary.nickname});
        return;
    }
    const StorageAddress address = session_.focusedAddress();
    const std::size_t slot = session_.focusedSlot;
    if (address.isInSave() && !session_.saveAdapter.canImportPokemon(hand.payload)) {
        context_.status = context_.text.format(TextId::GenerationIncompatible,
            {std::to_string(hand.payload.format), std::to_string(session_.saveAdapter.gameGeneration())});
        return;
    }
    if (!storage_.slots().loaded(address)) {
        context_.status = context_.text.get(TextId::BankBoxLoading);
        return;
    }
    if (!storage_.slots().available(address, slot)) {
        context_.status = context_.text.format(TextId::SlotUnavailable,
                                               {std::to_string(session_.saveAdapter.boxCapacity())});
        return;
    }
    SlotContents occupant;
    switch (storage_.slots().read(address, slot, occupant)) {
        case SlotState::Empty:
            placeHand(address, slot);
            return;
        case SlotState::Ready:
            swapHand(address, slot, std::move(occupant));
            return;
        case SlotState::PayloadPending:
            break;
    }
    if (!context_.signedIn()) {
        context_.status = context_.text.get(TextId::SignInAgain);
        return;
    }
    if (context_.loads.busy()) {
        return;
    }
    context_.status = context_.text.get(TextId::FetchingOccupant);
    startCloudFetch(LoadService::Operation::SwapCloud, slot, occupant.summary);
}


void HandController::completeCloudSwap(std::size_t slot, PokemonPayload occupantPayload) {
    const StorageAddress bank{StoragePane::Cloud, false};
    swapHand(bank, slot, {session_.cloud.summaries[slot], std::move(occupantPayload)});
}


void HandController::placeHand(StorageAddress address, std::size_t slot) {
    Hand& hand = session_.hand;
    storage_.slots().write(address, slot, {hand.summary, hand.payload});
    context_.status = context_.text.format(TextId::PokemonPlaced, {hand.summary.nickname});
    logSlot("drop placed", storage_.slots().describe(address, slot), hand.summary, hand.payload);
    hand = Hand{};
    ++session_.handGeneration;
}


void HandController::swapHand(StorageAddress address, std::size_t slot, SlotContents occupant) {
    Hand& hand = session_.hand;
    Logger::instance().info("drop swap: " + storage_.slots().describe(address, slot) + " incoming species "
                            + std::to_string(hand.summary.species) + " payload=" + payloadTag(hand.payload.data)
                            + " ; outgoing species " + std::to_string(occupant.summary.species) + " payload="
                            + payloadTag(occupant.payload.data));
    storage_.slots().write(address, slot, {hand.summary, hand.payload});
    hand.swapWith(storage_.slots().held(address, slot, std::move(occupant)));
    ++session_.handGeneration;
    context_.status = context_.text.format(TextId::PokemonSwapped, {hand.summary.nickname});
}


void HandController::startCloudFetch(LoadService::Operation operation, std::size_t slot,
                                        const PokemonSummary& summary) {
    context_.loads.beginPayloadFetch(operation,
        PayloadFetchRequest{slot, session_.cloudPosition(), summary, session_.handGeneration});
}


void HandController::returnHand() {
    if (!session_.hand.active) {
        return;
    }
    Hand hand = std::move(session_.hand);
    HeldPokemon held = hand;
    Logger::instance().info("returnHand: unwinding " + std::to_string(hand.swapHistory.size()) + " swap(s)");
    while (!hand.swapHistory.empty()) {
        storage_.restorePokemon(held);
        held = std::move(hand.swapHistory.back());
        hand.swapHistory.pop_back();
    }
    storage_.restorePokemon(held);

    context_.status = context_.text.format(TextId::ReturnedToSlot, {std::to_string(held.sourceIndex + 1)});
    logSlot("returnHand", storage_.slots().describe(held.source, held.sourceIndex), held.summary, held.payload);
    session_.hand = Hand{};
    ++session_.handGeneration;
}

