#pragma once

#include "network/ApiTypes.hpp"
#include "network/Json.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CloudJson {
json_t* slotJson(CloudSlot slot);
json_t* uploadEntry(const UploadPokemon& item);
json_t* claimEntry(const ClaimRequest& claim);
json_t* trainerJson(const TrainerInfo& trainer);
std::vector<UploadRejection> parseRejections(const Json::Document& body, const std::string& message);
std::optional<std::size_t> parseBoxEntry(json_t* entry, PokemonSummary& summary, std::vector<std::uint8_t>& payload);
ClaimState claimState(const std::string& status);

template <typename Item, typename Encode>
json_t* jsonArray(const std::vector<Item>& items, Encode encode) {
    json_t* array = json_array();
    for (const Item& item : items) {
        json_array_append_new(array, encode(item));
    }
    return array;
}
}
