#include "network/api/CloudApi.hpp"

#include "network/ApiReply.hpp"
#include "network/Json.hpp"
#include "network/api/CloudJson.hpp"
#include "platform/Logger.hpp"
#include "security/Base64.hpp"

#include <utility>

namespace {
constexpr const char* NotSignedIn = "Not signed in.";

std::string slotPath(CloudSlot slot) {
    return "/v1/pokemon/" + std::to_string(slot.boxPosition) + "/" + std::to_string(slot.slot);
}

std::string boxPath(std::uint16_t boxPosition) {
    return "/v1/boxes/" + std::to_string(boxPosition);
}

DeleteResult deleteResult(const HttpResponse& response, bool batch) {
    const ApiReply reply(response, "Delete rejected.");
    if (reply.accepted()) {
        return {RequestOutcome::Succeeded, "Deleted."};
    }
    const std::string code = reply.body().string("code");
    if (reply.status() == 404 && !batch && code == "SLOT_EMPTY") {
        return {RequestOutcome::Succeeded, "Already empty."};
    }
    if (reply.status() == 404 && batch && code.empty()) {
        return {RequestOutcome::Refused, "Batch delete is not supported by this server.", true};
    }
    return {reply.failure(), reply.error()};
}
}

UploadResult CloudApi::upload(const std::vector<UploadPokemon>& pokemon, const std::string& accessToken) {
    if (pokemon.empty() || pokemon.size() > BoxSlotCount || accessToken.empty()) {
        return {RequestOutcome::Refused, "The upload request is incomplete.", {}};
    }
    const std::string body = Json::encode({{"pokemon", CloudJson::jsonArray(pokemon, CloudJson::uploadEntry)}});
    Logger::instance().info("Uploading " + std::to_string(pokemon.size()) + " Pokemon (" + std::to_string(body.size())
                            + " bytes)");
    const ApiReply reply(http_.send(HttpMethod::Post, "/v1/pokemon/batch", body, accessToken,
                                    HttpClient::UploadTimeoutNanoseconds),
                         "The upload was rejected.");
    if (reply.accepted()) {
        const bool stored = reply.body().flag("stored") && json_is_integer(reply.body().field("count"));
        return stored ? UploadResult{RequestOutcome::Succeeded, "Upload complete.", {}}
                      : UploadResult{RequestOutcome::Unknown, "The server returned an incomplete upload response.", {}};
    }
    UploadResult result{reply.failure(), reply.error(), {}};
    if (reply.delivered()) {
        result.rejected = CloudJson::parseRejections(reply.body(), result.message);
        Logger::instance().warning("Pokemon batch upload refused (HTTP " + std::to_string(reply.status()) + ", "
                                   + std::to_string(result.rejected.size()) + " named): " + result.message);
    }
    return result;
}

DownloadResult CloudApi::download(CloudSlot slot, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, NotSignedIn, {}};
    }
    const ApiReply reply(http_.send(HttpMethod::Get, slotPath(slot), {}, accessToken), "Download rejected.");
    if (!reply.ok()) {
        return {false, reply.error(), {}};
    }
    PokemonPayload payload{reply.body().integer<std::uint8_t>("format", 0),
                           Base64::decode(reply.body().string("payloadBase64"))};
    if (!payload.known() || payload.format == 0) {
        return {false, "Payload missing.", {}};
    }
    return {true, "Download complete.", std::move(payload)};
}

DeleteResult CloudApi::remove(CloudSlot slot, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn};
    }
    return deleteResult(http_.send(HttpMethod::Delete, slotPath(slot), {}, accessToken), false);
}

DeleteResult CloudApi::remove(const std::vector<CloudSlot>& slots, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn};
    }
    if (slots.empty()) {
        return {RequestOutcome::Succeeded, "Nothing to delete."};
    }
    const std::string body = Json::encode({{"slots", CloudJson::jsonArray(slots, CloudJson::slotJson)}});
    return deleteResult(http_.send(HttpMethod::Post, "/v1/pokemon/delete", body, accessToken), true);
}

ClaimResult CloudApi::claim(const std::vector<ClaimRequest>& requests, const std::string& accessToken,
                            const std::optional<TrainerInfo>& trainer) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn, {}, {}};
    }
    if (requests.empty()) {
        return {RequestOutcome::Succeeded, "Nothing to claim.", {}, {}};
    }
    Json::Document body = Json::Document::object();
    body.set("slots", CloudJson::jsonArray(requests, CloudJson::claimEntry));
    if (trainer) {
        body.set("trainer", CloudJson::trainerJson(*trainer));
    }
    const ApiReply reply(http_.send(HttpMethod::Post, "/v1/pokemon/claim", body.dump(), accessToken), "Claim rejected.");
    if (!reply.ok()) {
        return {reply.failure(), reply.error(), {}, {}};
    }
    json_t* slots = reply.body().field("slots");
    if (json_array_size(slots) != requests.size()) {
        return {RequestOutcome::Unknown, "The server returned an invalid response.", {}, {}};
    }
    ClaimResult result{RequestOutcome::Succeeded, "Claimed.", {}, {}};
    for (std::size_t index = 0; index < requests.size(); ++index) {
        json_t* slot = json_array_get(slots, index);
        result.states.push_back(CloudJson::claimState(Json::string(slot, "status")));
        result.received.push_back(Base64::decode(Json::string(slot, "payloadBase64")));
    }
    return result;
}

MoveResult CloudApi::move(CloudSlot from, CloudSlot to, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn};
    }
    const std::string body = Json::encode({{"fromPosition", json_integer(from.boxPosition)},
                                           {"fromSlot", json_integer(from.slot)},
                                           {"toPosition", json_integer(to.boxPosition)},
                                           {"toSlot", json_integer(to.slot)}});
    const ApiReply reply(http_.send(HttpMethod::Put, "/v1/pokemon/move", body, accessToken), "Move rejected.");
    if (!reply.accepted()) {
        return {reply.failure(), reply.error()};
    }
    return reply.body().flag("moved")
        ? MoveResult{RequestOutcome::Succeeded, "Moved."}
        : MoveResult{RequestOutcome::Unknown, "The server returned an incomplete move response."};
}

BoxListResult CloudApi::listBox(std::uint16_t boxPosition, const std::string& accessToken) {
    BoxListResult result;
    if (accessToken.empty()) {
        result.message = NotSignedIn;
        return result;
    }
    const ApiReply reply(http_.send(HttpMethod::Get, boxPath(boxPosition), {}, accessToken), "Box listing rejected.");
    if (!reply.ok()) {
        result.message = reply.error();
        return result;
    }
    json_t* entries = reply.body().field("pokemon");
    for (std::size_t index = 0; index < json_array_size(entries); ++index) {
        PokemonSummary summary;
        std::vector<std::uint8_t> payload;
        const std::optional<std::size_t> slot = CloudJson::parseBoxEntry(json_array_get(entries, index), summary, payload);
        if (!slot) {
            continue;
        }
        if (!payload.empty()) {
            result.payloads[*slot] = {summary.format, std::move(payload)};
        }
        result.pokemon[*slot] = std::move(summary);
    }
    result.success = true;
    result.message = "Box loaded.";
    return result;
}

BoxNamesResult CloudApi::listBoxNames(const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, NotSignedIn, {}};
    }
    const ApiReply reply(http_.send(HttpMethod::Get, "/v1/boxes", {}, accessToken), "Box listing rejected.");
    if (!reply.ok()) {
        return {false, reply.error(), {}};
    }
    std::vector<BoxNameEntry> boxes;
    json_t* entries = reply.body().field("boxes");
    for (std::size_t index = 0; index < json_array_size(entries); ++index) {
        json_t* entry = json_array_get(entries, index);
        BoxNameEntry box{Json::integer<std::uint16_t>(entry, "position", 0), Json::string(entry, "name")};
        if (json_is_integer(json_object_get(entry, "position")) && !box.name.empty()) {
            boxes.push_back(std::move(box));
        }
    }
    return {true, "Boxes loaded.", std::move(boxes)};
}

RenameBoxResult CloudApi::renameBox(std::uint16_t boxPosition, const std::string& name, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, NotSignedIn, {}};
    }
    if (name.empty()) {
        return {false, "The box name cannot be blank.", {}};
    }
    const std::string body = Json::encode({{"name", json_string(name.c_str())}});
    const ApiReply reply(http_.send(HttpMethod::Put, boxPath(boxPosition) + "/name", body, accessToken),
                         "Rename rejected.");
    if (!reply.ok()) {
        return {false, reply.error(), {}};
    }
    const std::string storedName = reply.body().string("name");
    return {true, "Box renamed.", storedName.empty() ? name : storedName};
}
