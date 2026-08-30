#include "bank/TrashCanBox.hpp"

bool TrashCanBox::empty() const {
    return count() == 0;
}

std::size_t TrashCanBox::count() const {
    std::size_t total = 0;
    for (const PokemonSummary& mon : summaries_) {
        if (mon.species != 0) {
            ++total;
        }
    }
    return total;
}

void TrashCanBox::reset() {
    summaries_ = {};
    payloads_ = {};
}
