#pragma once

#include "bank/BankTypes.hpp"

struct StorageAddress {
    StoragePane pane = StoragePane::Local;
    bool trash = false;

    constexpr bool isCloudBank() const { return pane == StoragePane::Cloud && !trash; }
    constexpr bool isTrashCan() const { return pane == StoragePane::Cloud && trash; }
};

constexpr bool operator==(StorageAddress left, StorageAddress right) {
    return left.pane == right.pane && left.trash == right.trash;
}

constexpr bool operator!=(StorageAddress left, StorageAddress right) {
    return !(left == right);
}

constexpr HandSource handSourceOf(StorageAddress address) {
    switch (address.pane) {
        case StoragePane::Local:
            return HandSource::Local;
        case StoragePane::Party:
            return HandSource::Party;
        case StoragePane::Cloud:
            break;
    }
    return HandSource::Cloud;
}
