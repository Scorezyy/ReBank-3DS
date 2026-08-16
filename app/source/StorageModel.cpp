#include "StorageModel.hpp"

#include <algorithm>
#include <utility>

void StorageModel::load(std::array<PokemonSummary, 30> pokemon) {
    pokemon_ = std::move(pokemon);
    clearSelection();
}

void StorageModel::clear() {
    pokemon_ = {};
    clearSelection();
}

void StorageModel::set(std::size_t slot, PokemonSummary pokemon) {
    if (slot < pokemon_.size()) {
        pokemon_[slot] = std::move(pokemon);
    }
}

void StorageModel::toggle(std::size_t slot) {
    if (slot < selected_.size()) {
        selected_[slot] = !selected_[slot];
    }
}

void StorageModel::clearSelection() {
    selected_.fill(false);
}

bool StorageModel::selected(std::size_t slot) const {
    return slot < selected_.size() && selected_[slot];
}

std::size_t StorageModel::selectedCount() const {
    return std::count(selected_.begin(), selected_.end(), true);
}

const PokemonSummary& StorageModel::pokemon(std::size_t slot) const {
    return pokemon_.at(slot);
}