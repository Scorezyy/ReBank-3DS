#include "network/api/CloudJson.hpp"

#include "save/pokemon/PokemonTransfer.hpp"
#include "security/Base64.hpp"
#include "security/Hex.hpp"
#include "security/Hmac.hpp"

#include <pkx/PKX.hpp>

#include <cstdio>
#include <utility>

namespace {
json_t* stringOr(const std::string& value, const char* fallback) {
    return json_string(value.empty() ? fallback : value.c_str());
}

std::optional<UploadRejection> parseLegacyRejection(const std::string& message) {
    unsigned boxPosition = 0;
    unsigned slot = 0;
    int consumed = 0;
    if (std::sscanf(message.c_str(), "Bank %u, Slot %u: %n", &boxPosition, &slot, &consumed) != 2 || consumed == 0
        || boxPosition == 0 || slot == 0 || slot > BoxSlotCount) {
        return std::nullopt;
    }
    return UploadRejection{{static_cast<std::uint16_t>(boxPosition), static_cast<std::uint8_t>(slot)},
                           message.substr(static_cast<std::size_t>(consumed))};
}

void enrichFromPayload(PokemonSummary& summary, const std::vector<std::uint8_t>& payload) {
    const pksm::Generation generation = PokemonTransfer::generationFromFormat(summary.format);
    if (payload.empty() || generation == pksm::Generation::UNUSED) {
        return;
    }
    std::vector<std::uint8_t> buffer(payload);
    const auto pkx = pksm::PKX::getPKM(generation, buffer.data(), buffer.size(), false);
    if (!pkx || static_cast<std::uint16_t>(pkx->species()) == 0) {
        return;
    }
    summary.type1 = pkx->type1();
    summary.type2 = pkx->type2();
    summary.originGame = pkx->version();
    summary.language = pkx->language();
    summary.moves = {pkx->move(0), pkx->move(1), pkx->move(2), pkx->move(3)};
    summary.ability = pkx->ability();
    summary.nature = pkx->nature();
    summary.gender = pkx->gender();
}
}

namespace CloudJson {
json_t* slotJson(CloudSlot slot) {
    json_t* entry = json_object();
    json_object_set_new(entry, "boxPosition", json_integer(slot.boxPosition));
    json_object_set_new(entry, "slot", json_integer(slot.slot));
    return entry;
}

json_t* uploadEntry(const UploadPokemon& item) {
    const PokemonSummary& summary = item.summary;
    json_t* entry = slotJson(item.destination);
    json_object_set_new(entry, "format", json_integer(item.payload.format));
    json_object_set_new(entry, "payloadBase64", json_string(Base64::encode(item.payload.data).c_str()));
    json_object_set_new(entry, "species", json_integer(summary.species));
    json_object_set_new(entry, "nickname", stringOr(summary.nickname, "Pokemon"));
    json_object_set_new(entry, "trainerName", stringOr(summary.trainerName, "Unknown"));
    json_object_set_new(entry, "level", json_integer(summary.level));
    json_object_set_new(entry, "gameCode", stringOr(summary.gameCode, "unknown"));
    json_object_set_new(entry, "shiny", json_boolean(summary.shiny));
    json_object_set_new(entry, "heldItem", json_integer(summary.heldItem));
    json_object_set_new(entry, "replace", json_boolean(item.replaceOccupant));
    return entry;
}

json_t* claimEntry(const ClaimRequest& claim) {
    json_t* entry = slotJson(claim.slot);
    json_object_set_new(entry, "sha256", json_string(Hex::encode(Hmac::sha256(claim.payload)).c_str()));
    return entry;
}

json_t* trainerJson(const TrainerInfo& trainer) {
    json_t* owner = json_object();
    json_object_set_new(owner, "format", json_integer(trainer.format));
    json_object_set_new(owner, "name", json_string(trainer.name.c_str()));
    json_object_set_new(owner, "gender", json_integer(trainer.gender));
    json_object_set_new(owner, "tid", json_integer(trainer.tid));
    json_object_set_new(owner, "sid", json_integer(trainer.sid));
    json_object_set_new(owner, "language", json_integer(trainer.language));
    json_object_set_new(owner, "country", json_integer(trainer.country));
    json_object_set_new(owner, "region", json_integer(trainer.region));
    json_object_set_new(owner, "consoleRegion", json_integer(trainer.consoleRegion));
    return owner;
}

std::vector<UploadRejection> parseRejections(const Json::Document& body, const std::string& message) {
    std::vector<UploadRejection> rejections;
    json_t* rejected = body.field("rejected");
    for (std::size_t index = 0; index < json_array_size(rejected); ++index) {
        json_t* entry = json_array_get(rejected, index);
        if (json_is_object(entry)) {
            rejections.push_back({{Json::integer<std::uint16_t>(entry, "boxPosition", 0),
                                   Json::integer<std::uint8_t>(entry, "slot", 0)},
                                  Json::string(entry, "reason")});
        }
    }
    if (rejections.empty()) {
        if (std::optional<UploadRejection> legacy = parseLegacyRejection(message)) {
            rejections.push_back(std::move(*legacy));
        }
    }
    return rejections;
}

std::optional<std::size_t> parseBoxEntry(json_t* entry, PokemonSummary& summary, std::vector<std::uint8_t>& payload) {
    if (!json_is_object(entry)) {
        return std::nullopt;
    }
    const int slot = Json::integer<int>(entry, "slot", 0);
    if (slot < 1 || slot > static_cast<int>(BoxSlotCount)) {
        return std::nullopt;
    }
    summary.species = Json::integer<std::uint16_t>(entry, "species", 0);
    summary.level = Json::integer<std::uint8_t>(entry, "level", 0);
    summary.nickname = Json::string(entry, "nickname");
    summary.trainerName = Json::string(entry, "trainerName");
    summary.gameCode = Json::string(entry, "gameCode");
    summary.format = Json::integer<std::uint8_t>(entry, "format", 0);
    summary.shiny = Json::flag(entry, "shiny");
    const bool firstGeneration = PokemonTransfer::generationFromFormat(summary.format) == pksm::Generation::ONE;
    summary.heldItem = firstGeneration ? std::uint16_t{0} : Json::integer<std::uint16_t>(entry, "heldItem", 0);
    payload = Base64::decode(Json::string(entry, "payloadBase64"));
    enrichFromPayload(summary, payload);
    return static_cast<std::size_t>(slot - 1);
}

ClaimState claimState(const std::string& status) {
    if (status == "claimed") {
        return ClaimState::Claimed;
    }
    if (status == "locked") {
        return ClaimState::Locked;
    }
    if (status == "empty") {
        return ClaimState::Empty;
    }
    return ClaimState::Changed;
}
}
