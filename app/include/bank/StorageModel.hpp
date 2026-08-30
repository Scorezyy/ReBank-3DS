#pragma once

#include "save/adapter/SaveAdapter.hpp"

#include <array>
#include <cstddef>

class StorageModel {
public:
    void load(std::array<PokemonSummary, 30> pokemon);
    void clear();
    void set(std::size_t slot, PokemonSummary pokemon);
    void toggle(std::size_t slot);
    void clearSelection();
    bool selected(std::size_t slot) const;
    std::size_t selectedCount() const;
    const PokemonSummary& pokemon(std::size_t slot) const;

private:
    std::array<PokemonSummary, 30> pokemon_{};
    std::array<bool, 30> selected_{};
};
