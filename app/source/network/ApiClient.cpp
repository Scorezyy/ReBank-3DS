#include "network/ApiClient.hpp"

#include "BuildConfig.hpp"
#include "core/Base64.hpp"
#include "core/Hex.hpp"
#include "core/Hmac.hpp"
#include "core/Logger.hpp"
#include "core/RequestSigning.hpp"
#include "core/ServerConfig.hpp"
#include "network/Json.hpp"
#include "save/pokemon/PokemonTransfer.hpp"

#include <3ds.h>
#include <pkx/PKX.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace {
constexpr std::size_t MaximumResponseSize = 64 * 1024;
constexpr std::size_t MaximumRequestSize = 0xF000;
constexpr std::size_t MaximumUpdateSize = 5 * 1024 * 1024;
constexpr std::size_t DownloadChunkSize = 32 * 1024;
constexpr std::size_t MaximumCertificateSize = 16 * 1024;
constexpr const char* TrustedRootPaths[] = {
    "romfs:/assets/tls/usertrust-rsa.der",
    "romfs:/assets/tls/sectigo-r46.der",
    "romfs:/assets/tls/isrg-x1.der",
};
constexpr const char* InvalidResponse = "The server returned an invalid response.";
constexpr const char* NotSignedIn = "Not signed in.";
constexpr std::string_view CiaAssetName = "ReBank.cia";
constexpr std::string_view ThreeDsxAssetName = "ReBank.3dsx";
const std::string JsonContentType = "application/json";

std::string resultCode(Result result) {
    return Hex::resultCode(static_cast<std::uint32_t>(result));
}

bool isSuccessStatus(std::uint32_t status) {
    return status >= 200 && status < 300;
}

std::string slotPath(CloudSlot slot) {
    return "/v1/pokemon/" + std::to_string(slot.boxPosition) + "/" + std::to_string(slot.slot);
}

std::string boxPath(std::uint16_t boxPosition) {
    return "/v1/boxes/" + std::to_string(boxPosition);
}

json_t* slotJson(CloudSlot slot) {
    json_t* entry = json_object();
    json_object_set_new(entry, "boxPosition", json_integer(slot.boxPosition));
    json_object_set_new(entry, "slot", json_integer(slot.slot));
    return entry;
}

json_t* stringOr(const std::string& value, const char* fallback) {
    return json_string(value.empty() ? fallback : value.c_str());
}

std::vector<u8> readCertificate(const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return {};
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::rewind(file);
    if (size <= 0 || static_cast<std::size_t>(size) > MaximumCertificateSize) {
        std::fclose(file);
        return {};
    }
    std::vector<u8> contents(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(contents.data(), 1, contents.size(), file);
    std::fclose(file);
    return read == contents.size() ? contents : std::vector<u8>{};
}

const std::vector<std::vector<u8>>& trustedRootCertificates() {
    static const std::vector<std::vector<u8>> certificates = [] {
        std::vector<std::vector<u8>> loaded;
        for (const char* path : TrustedRootPaths) {
            std::vector<u8> certificate = readCertificate(path);
            if (certificate.empty()) {
                Logger::instance().error(std::string("Trusted root missing: ") + path);
                continue;
            }
            loaded.push_back(std::move(certificate));
        }
        return loaded;
    }();
    return certificates;
}

Result openTrustedContext(httpcContext& context, HTTPC_RequestMethod method, const std::string& url,
                          std::string& outMessage) {
    const Result result = httpcOpenContext(&context, method, url.c_str(), 0);
    if (R_FAILED(result)) {
        outMessage = "Connection setup failed (" + resultCode(result) + ").";
        return result;
    }
    for (const auto& certificate : trustedRootCertificates()) {
        const Result added = httpcAddTrustedRootCA(&context, certificate.data(), static_cast<u32>(certificate.size()));
        if (R_FAILED(added)) {
            Logger::instance().error("Trusted root rejected: " + resultCode(added));
        }
    }
    return result;
}

bool validUpdateText(const std::string& value, bool tag) {
    if (value.empty() || value.size() > 48 || (tag && value[0] != 'v')) {
        return false;
    }
    return std::all_of(value.begin() + (tag ? 1 : 0), value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '.' || character == '-';
    });
}

bool validSha256(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isdigit(character) || (character >= 'a' && character <= 'f');
    });
}

struct Envelope {
    Json::Document document;
    std::string error;

    bool ok() const { return error.empty(); }
};

Envelope parseEnvelope(std::uint32_t status, const std::string& body, const char* rejectionFallback) {
    Envelope envelope{Json::Document::parse(body), {}};
    if (!envelope.document.isObject()) {
        envelope.error = InvalidResponse;
    } else if (!isSuccessStatus(status)) {
        const std::string message = envelope.document.string("message");
        envelope.error = message.empty() ? rejectionFallback : message;
    }
    return envelope;
}

RequestOutcome refusalOutcome(std::uint32_t status, const Json::Document& reply) {
    return reply.string("code").empty() && status >= 500 ? RequestOutcome::Unknown : RequestOutcome::Refused;
}

std::string replyMessage(const Json::Document& reply, const char* fallback) {
    const std::string message = reply.string("message");
    return message.empty() ? fallback : message;
}

bool parseLegacyRejection(const std::string& message, UploadRejection& rejection) {
    unsigned boxPosition = 0;
    unsigned slot = 0;
    int consumed = 0;
    if (std::sscanf(message.c_str(), "Bank %u, Slot %u: %n", &boxPosition, &slot, &consumed) != 2 || consumed == 0
        || boxPosition == 0 || slot == 0 || slot > BoxSlotCount) {
        return false;
    }
    rejection = {{static_cast<std::uint16_t>(boxPosition), static_cast<std::uint8_t>(slot)},
                 message.substr(static_cast<std::size_t>(consumed))};
    return true;
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

std::string uploadBody(const std::vector<UploadPokemon>& pokemon) {
    json_t* entries = json_array();
    for (const UploadPokemon& item : pokemon) {
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
        json_array_append_new(entries, entry);
    }
    Json::Document body = Json::Document::object();
    body.set("pokemon", entries);
    return body.dump();
}

std::vector<UploadRejection> parseRejections(const Json::Document& reply, const std::string& message) {
    std::vector<UploadRejection> rejections;
    json_t* rejected = reply.field("rejected");
    for (std::size_t index = 0; index < json_array_size(rejected); ++index) {
        json_t* entry = json_array_get(rejected, index);
        if (json_is_object(entry)) {
            rejections.push_back({{Json::integer<std::uint16_t>(entry, "boxPosition", 0),
                                   Json::integer<std::uint8_t>(entry, "slot", 0)},
                                  Json::string(entry, "reason")});
        }
    }
    UploadRejection legacy;
    if (rejections.empty() && parseLegacyRejection(message, legacy)) {
        rejections.push_back(std::move(legacy));
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
    summary.heldItem = PokemonTransfer::generationFromFormat(summary.format) == pksm::Generation::ONE
        ? std::uint16_t{0}
        : Json::integer<std::uint16_t>(entry, "heldItem", 0);
    payload = Base64::decode(Json::string(entry, "payloadBase64"));
    enrichFromPayload(summary, payload);
    return static_cast<std::size_t>(slot - 1);
}

AuthResult parseAuthResponse(std::uint32_t status, const std::string& response) {
    const int httpStatus = static_cast<int>(status);
    const Json::Document root = Json::Document::parse(response);
    if (!root.isObject()) {
        return {false, InvalidResponse, {}, false, httpStatus};
    }
    if (!isSuccessStatus(status)) {
        return {false, replyMessage(root, "The server rejected the request."), {}, false, httpStatus};
    }
    if (status == 202) {
        return {true, "Password reset request accepted.", {}, false, httpStatus};
    }
    json_t* account = root.field("account");
    AccountSession session{
        Json::string(account, "id"),
        root.string("accessToken"),
        root.string("refreshToken"),
        Json::integer<std::uint16_t>(account, "boxLimit", 50)
    };
    if (session.accountId.empty() || session.accessToken.empty() || session.refreshToken.empty()) {
        return {false, "The server returned an incomplete session.", {}, false, httpStatus};
    }
    return {true, "Authentication succeeded.", std::move(session), false, httpStatus};
}

std::optional<UpdateAsset> parseUpdateAsset(json_t* asset) {
    UpdateAsset parsed{Json::string(asset, "sha256"), Json::string(asset, "signature"), 0};
    const json_int_t size = Json::integer<json_int_t>(asset, "size", 0);
    if (!validSha256(parsed.sha256) || size <= 0 || size > static_cast<json_int_t>(MaximumUpdateSize)) {
        return std::nullopt;
    }
    parsed.size = static_cast<std::uint32_t>(size);
    return parsed;
}
}

ApiClient::ApiClient() {
    LightLock_Init(&clockLock_);
    initialized_ = R_SUCCEEDED(httpcInit(0x10000));
    if (!initialized_) {
        Logger::instance().error("HTTP service initialization failed");
    }
}

ApiClient::~ApiClient() {
    if (initialized_) {
        httpcExit();
    }
}

bool ApiClient::consumeSessionRejected() {
    return sessionRejected_.exchange(false);
}

AuthResult ApiClient::login(const std::string& username, const std::string& password) {
    Json::Document body = Json::Document::object();
    body.set("username", json_string(username.c_str())).set("password", json_string(password.c_str()));
    return postAuth("/v1/auth/login", body.dump());
}

AuthResult ApiClient::registerAccount(const std::string& username, const std::string& email,
                                      const std::string& password, const std::string& deviceFingerprint) {
    if (deviceFingerprint.empty()) {
        return {false, "This console's identity could not be verified. Registration requires a genuine, unmodified 3DS.",
                {}, false, 0};
    }
    Json::Document body = Json::Document::object();
    body.set("username", json_string(username.c_str()))
        .set("email", json_string(email.c_str()))
        .set("password", json_string(password.c_str()))
        .set("deviceFingerprint", json_string(deviceFingerprint.c_str()));
    return postAuth("/v1/auth/register", body.dump());
}

AuthResult ApiClient::refresh(const std::string& refreshToken) {
    Json::Document body = Json::Document::object();
    body.set("refreshToken", json_string(refreshToken.c_str()));
    return postAuth("/v1/auth/refresh", body.dump());
}

AuthResult ApiClient::requestPasswordReset(const std::string& email) {
    Json::Document body = Json::Document::object();
    body.set("email", json_string(email.c_str()));
    return postAuth("/v1/auth/password-reset", body.dump());
}

AuthResult ApiClient::postAuth(const std::string& path, const std::string& body) {
    const HttpResult response = request(Method::Post, path, body);
    if (!response.success) {
        return {false, response.message, {}, true};
    }
    return parseAuthResponse(response.status, response.body);
}

UploadResult ApiClient::uploadPokemon(const std::vector<UploadPokemon>& pokemon, const std::string& accessToken) {
    if (pokemon.empty() || pokemon.size() > BoxSlotCount || accessToken.empty()) {
        return {RequestOutcome::Refused, "The upload request is incomplete.", {}};
    }
    const std::string body = uploadBody(pokemon);
    Logger::instance().info("Uploading " + std::to_string(pokemon.size()) + " Pokemon ("
                            + std::to_string(body.size()) + " bytes)");
    const HttpResult response = request(Method::Post, "/v1/pokemon/batch", body, accessToken,
                                        UploadTimeoutNanoseconds);
    if (!response.success) {
        return {response.failureOutcome(), response.message, {}};
    }
    const Json::Document reply = Json::Document::parse(response.body);
    if (isSuccessStatus(response.status)) {
        const bool accepted = reply.flag("stored") && json_is_integer(reply.field("count"));
        return accepted ? UploadResult{RequestOutcome::Succeeded, "Upload complete.", {}}
                        : UploadResult{RequestOutcome::Unknown, "The server returned an incomplete upload response.", {}};
    }
    UploadResult result{refusalOutcome(response.status, reply), replyMessage(reply, "The upload was rejected."), {}};
    result.rejected = parseRejections(reply, result.message);
    Logger::instance().warning("Pokemon batch upload refused (HTTP " + std::to_string(response.status) + ", "
                               + std::to_string(result.rejected.size()) + " named): " + result.message);
    return result;
}

DownloadResult ApiClient::downloadPokemon(CloudSlot slot, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, NotSignedIn, {}};
    }
    const HttpResult response = request(Method::Get, slotPath(slot), {}, accessToken);
    if (!response.success) {
        return {false, response.message, {}};
    }
    const Envelope envelope = parseEnvelope(response.status, response.body, "Download rejected.");
    if (!envelope.ok()) {
        return {false, envelope.error, {}};
    }
    PokemonPayload payload{envelope.document.integer<std::uint8_t>("format", 0),
                           Base64::decode(envelope.document.string("payloadBase64"))};
    if (!payload.known() || payload.format == 0) {
        return {false, "Payload missing.", {}};
    }
    return {true, "Download complete.", std::move(payload)};
}

DeleteResult ApiClient::deleteResult(const HttpResult& response, bool batch) {
    if (!response.success) {
        return {response.failureOutcome(), response.message};
    }
    if (isSuccessStatus(response.status)) {
        return {RequestOutcome::Succeeded, "Deleted."};
    }
    const Json::Document reply = Json::Document::parse(response.body);
    const std::string code = reply.string("code");
    if (response.status == 404 && !batch && code == "SLOT_EMPTY") {
        return {RequestOutcome::Succeeded, "Already empty."};
    }
    if (response.status == 404 && batch && code.empty()) {
        return {RequestOutcome::Refused, "Batch delete is not supported by this server.", true};
    }
    return {refusalOutcome(response.status, reply), replyMessage(reply, "Delete rejected.")};
}

DeleteResult ApiClient::deleteCloudPokemon(CloudSlot slot, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn};
    }
    return deleteResult(request(Method::Delete, slotPath(slot), {}, accessToken), false);
}

DeleteResult ApiClient::deleteCloudPokemonBatch(const std::vector<CloudSlot>& slots, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn};
    }
    if (slots.empty()) {
        return {RequestOutcome::Succeeded, "Nothing to delete."};
    }
    json_t* entries = json_array();
    for (const CloudSlot& slot : slots) {
        json_array_append_new(entries, slotJson(slot));
    }
    Json::Document body = Json::Document::object();
    body.set("slots", entries);
    return deleteResult(request(Method::Post, "/v1/pokemon/delete", body.dump(), accessToken), true);
}

ClaimResult ApiClient::claimCloudPokemon(const std::vector<ClaimRequest>& requests, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn, {}};
    }
    if (requests.empty()) {
        return {RequestOutcome::Succeeded, "Nothing to claim.", {}};
    }
    json_t* entries = json_array();
    for (const ClaimRequest& claim : requests) {
        json_t* entry = slotJson(claim.slot);
        const std::string digest = Hex::encode(Hmac::sha256(claim.payload));
        json_object_set_new(entry, "sha256", json_string(digest.c_str()));
        json_array_append_new(entries, entry);
    }
    Json::Document body = Json::Document::object();
    body.set("slots", entries);

    const HttpResult response = request(Method::Post, "/v1/pokemon/claim", body.dump(), accessToken);
    if (!response.success) {
        return {response.failureOutcome(), response.message, {}};
    }
    const Envelope envelope = parseEnvelope(response.status, response.body, "Claim rejected.");
    if (!envelope.ok()) {
        return {refusalOutcome(response.status, envelope.document), envelope.error, {}};
    }
    json_t* slots = envelope.document.field("slots");
    if (json_array_size(slots) != requests.size()) {
        return {RequestOutcome::Unknown, InvalidResponse, {}};
    }
    ClaimResult result{RequestOutcome::Succeeded, "Claimed.", {}};
    for (std::size_t index = 0; index < requests.size(); ++index) {
        const std::string status = Json::string(json_array_get(slots, index), "status");
        result.states.push_back(status == "claimed" ? ClaimState::Claimed
                                : status == "locked" ? ClaimState::Locked
                                : status == "empty"  ? ClaimState::Empty
                                                     : ClaimState::Changed);
    }
    return result;
}

MoveResult ApiClient::moveCloudPokemon(CloudSlot from, CloudSlot to, const std::string& accessToken) {
    if (accessToken.empty()) {
        return {RequestOutcome::Refused, NotSignedIn};
    }
    Json::Document body = Json::Document::object();
    body.set("fromPosition", json_integer(from.boxPosition))
        .set("fromSlot", json_integer(from.slot))
        .set("toPosition", json_integer(to.boxPosition))
        .set("toSlot", json_integer(to.slot));

    const HttpResult response = request(Method::Put, "/v1/pokemon/move", body.dump(), accessToken);
    if (!response.success) {
        return {response.failureOutcome(), response.message};
    }
    const Json::Document reply = Json::Document::parse(response.body);
    if (!isSuccessStatus(response.status)) {
        return {refusalOutcome(response.status, reply), replyMessage(reply, "Move rejected.")};
    }
    return reply.flag("moved") ? MoveResult{RequestOutcome::Succeeded, "Moved."}
                               : MoveResult{RequestOutcome::Unknown, "The server returned an incomplete move response."};
}

BoxListResult ApiClient::listCloudBox(std::uint16_t boxPosition, const std::string& accessToken) {
    BoxListResult result;
    if (accessToken.empty()) {
        result.message = NotSignedIn;
        return result;
    }
    const HttpResult response = request(Method::Get, boxPath(boxPosition), {}, accessToken);
    if (!response.success) {
        result.message = response.message;
        return result;
    }
    const Envelope envelope = parseEnvelope(response.status, response.body, "Box listing rejected.");
    if (!envelope.ok()) {
        result.message = envelope.error;
        return result;
    }
    json_t* array = envelope.document.field("pokemon");
    for (std::size_t index = 0; index < json_array_size(array); ++index) {
        PokemonSummary summary;
        std::vector<std::uint8_t> payload;
        const std::optional<std::size_t> slot = parseBoxEntry(json_array_get(array, index), summary, payload);
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

RenameBoxResult ApiClient::renameBox(std::uint16_t boxPosition, const std::string& name,
                                     const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, NotSignedIn, {}};
    }
    if (name.empty()) {
        return {false, "The box name cannot be blank.", {}};
    }
    Json::Document body = Json::Document::object();
    body.set("name", json_string(name.c_str()));

    const HttpResult response = request(Method::Put, boxPath(boxPosition) + "/name", body.dump(), accessToken);
    if (!response.success) {
        return {false, response.message, {}};
    }
    const Envelope envelope = parseEnvelope(response.status, response.body, "Rename rejected.");
    if (!envelope.ok()) {
        return {false, envelope.error, {}};
    }
    const std::string storedName = envelope.document.string("name");
    return {true, "Box renamed.", storedName.empty() ? name : storedName};
}

BoxNamesResult ApiClient::listBoxNames(const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, NotSignedIn, {}};
    }
    const HttpResult response = request(Method::Get, "/v1/boxes", {}, accessToken);
    if (!response.success) {
        return {false, response.message, {}};
    }
    const Envelope envelope = parseEnvelope(response.status, response.body, "Box listing rejected.");
    if (!envelope.ok()) {
        return {false, envelope.error, {}};
    }
    std::vector<BoxNameEntry> boxes;
    json_t* array = envelope.document.field("boxes");
    for (std::size_t index = 0; index < json_array_size(array); ++index) {
        json_t* entry = json_array_get(array, index);
        if (!json_is_integer(json_object_get(entry, "position"))) {
            continue;
        }
        BoxNameEntry item{Json::integer<std::uint16_t>(entry, "position", 0), Json::string(entry, "name")};
        if (!item.name.empty()) {
            boxes.push_back(std::move(item));
        }
    }
    return {true, "Boxes loaded.", std::move(boxes)};
}

ClientUpdate ApiClient::latestClientUpdate() {
    ClientUpdate update;
    const HttpResult response = request(Method::Get, "/v1/client/update");
    if (!response.success) {
        update.message = response.message;
        return update;
    }
    const Json::Document root = Json::Document::parse(response.body);
    json_t* assets = root.field("assets");
    if (response.status != 200 || !root.isObject() || !json_is_array(assets)) {
        update.message = "The server returned invalid update information.";
        return update;
    }
    update.tag = root.string("tag");
    update.version = root.string("version");
    for (std::size_t index = 0; index < json_array_size(assets); ++index) {
        json_t* asset = json_array_get(assets, index);
        const std::string name = Json::string(asset, "name");
        std::optional<UpdateAsset> parsed = parseUpdateAsset(asset);
        if (!parsed) {
            continue;
        }
        if (name == CiaAssetName) {
            update.cia = std::move(*parsed);
        } else if (name == ThreeDsxAssetName) {
            update.threeDsx = std::move(*parsed);
        }
    }
    if (!validUpdateText(update.tag, true) || !validUpdateText(update.version, false)
        || update.cia.sha256.empty() || update.threeDsx.sha256.empty()) {
        update.message = "The server returned incomplete update information.";
        return update;
    }
    update.success = true;
    update.message = "Update information loaded.";
    return update;
}

FileDownloadResult ApiClient::downloadClientUpdate(const std::string& tag, const std::string& assetName,
                                                   const std::string& destination, std::uint32_t expectedSize) {
    if (!initialized_ || !validUpdateText(tag, true)
        || (assetName != CiaAssetName && assetName != ThreeDsxAssetName)
        || expectedSize == 0 || expectedSize > MaximumUpdateSize) {
        return {false, "The update download request is invalid.", 0};
    }
    const std::string url = ServerConfig::baseUrl() + "/v1/client/update/" + tag + "/" + assetName;
    httpcContext context{};
    std::string openError;
    Result result = openTrustedContext(context, HTTPC_METHOD_GET, url, openError);
    if (R_FAILED(result)) {
        return {false, openError, 0};
    }
    result = httpcAddRequestHeaderField(&context, "Accept", "application/octet-stream");
    if (R_SUCCEEDED(result)) {
        result = httpcBeginRequest(&context);
    }
    u32 status = 0;
    if (R_SUCCEEDED(result)) {
        result = httpcGetResponseStatusCodeTimeout(&context, &status, DefaultTimeoutNanoseconds);
    }
    if (R_FAILED(result) || status != 200) {
        httpcCloseContext(&context);
        return {false, "Update connection failed (" + resultCode(result) + ").", 0};
    }

    FILE* file = std::fopen(destination.c_str(), "wb");
    if (!file) {
        httpcCloseContext(&context);
        return {false, "The temporary update file could not be created.", 0};
    }
    std::vector<u8> buffer(DownloadChunkSize);
    u32 total = 0;
    do {
        u32 before = 0;
        u32 after = 0;
        u32 contentSize = 0;
        httpcGetDownloadSizeState(&context, &before, &contentSize);
        result = httpcReceiveDataTimeout(&context, buffer.data(), buffer.size(), DefaultTimeoutNanoseconds);
        httpcGetDownloadSizeState(&context, &after, &contentSize);
        const u32 received = after >= before ? after - before : 0;
        if (received > buffer.size() || total + received > expectedSize
            || (received > 0 && std::fwrite(buffer.data(), 1, received, file) != received)) {
            result = static_cast<Result>(-1);
            break;
        }
        total += received;
    } while (static_cast<u32>(result) == HTTPC_RESULTCODE_DOWNLOADPENDING);
    const bool closed = std::fclose(file) == 0;
    httpcCloseContext(&context);
    if (R_FAILED(result) || !closed || total != expectedSize) {
        std::remove(destination.c_str());
        return {false, "The update download was incomplete.", total};
    }
    return {true, "Update downloaded.", total};
}

void ApiClient::syncClock() {
    constexpr std::uint64_t RetryIntervalMs = 30'000;
    const std::uint64_t now = osGetTime();
    LightLock_Lock(&clockLock_);
    const bool skip = clockDeltaMs_.has_value()
        || (lastClockSyncAttemptMs_ != 0 && now - lastClockSyncAttemptMs_ < RetryIntervalMs);
    if (!skip) {
        lastClockSyncAttemptMs_ = now;
    }
    LightLock_Unlock(&clockLock_);
    if (skip) {
        return;
    }
    const std::int64_t localMsBeforeSync = static_cast<std::int64_t>(osGetTime());
    const HttpResult response = request(Method::Get, "/v1/time");
    if (!response.success || response.status != 200) {
        return;
    }
    const Json::Document root = Json::Document::parse(response.body);
    if (!json_is_integer(root.field("unixSeconds"))) {
        return;
    }
    const std::int64_t delta = root.integer<std::int64_t>("unixSeconds", 0) * 1000 - localMsBeforeSync;
    LightLock_Lock(&clockLock_);
    clockDeltaMs_ = delta;
    LightLock_Unlock(&clockLock_);
    Logger::instance().info("Clock synced with server, delta=" + std::to_string(delta) + "ms");
}

std::uint64_t ApiClient::signedTimestampSeconds() {
    const std::int64_t localMs = static_cast<std::int64_t>(osGetTime());
    LightLock_Lock(&clockLock_);
    const std::optional<std::int64_t> delta = clockDeltaMs_;
    LightLock_Unlock(&clockLock_);
    if (delta) {
        return static_cast<std::uint64_t>((localMs + *delta) / 1000);
    }
    constexpr std::uint64_t NtpToUnixEpochOffsetSeconds = 2208988800ULL;
    std::int64_t timeOffsetMs = 0;
    CFGU_GetConfigInfoBlk2(sizeof(timeOffsetMs), 0x00030001, &timeOffsetMs);
    return static_cast<std::uint64_t>(localMs - timeOffsetMs) / 1000ULL - NtpToUnixEpochOffsetSeconds;
}

ApiClient::HttpResult ApiClient::request(Method method, const std::string& path, const std::string& body,
                                         const std::string& accessToken, std::uint64_t timeoutNanoseconds) {
    constexpr int MaximumAttempts = 3;
    constexpr std::uint32_t MaximumWaitSeconds = 20;
    for (int attempt = 1;; ++attempt) {
        HttpResult result = requestOnce(method, path, body, accessToken, timeoutNanoseconds);
        if (!result.success || result.status != 429 || attempt >= MaximumAttempts) {
            return result;
        }
        const std::uint32_t wait = std::clamp<std::uint32_t>(result.retryAfterSeconds, 1, MaximumWaitSeconds);
        Logger::instance().warning("HTTP 429 for " + path + ", retrying in " + std::to_string(wait) + "s");
        svcSleepThread(static_cast<s64>(wait) * 1'000'000'000LL);
    }
}

ApiClient::HttpResult ApiClient::requestOnce(Method method, const std::string& path, const std::string& body,
                                             const std::string& accessToken, std::uint64_t timeoutNanoseconds) {
    if (!initialized_) {
        return {false, 0, {}, "The network service is unavailable."};
    }
    if (body.size() > MaximumRequestSize) {
        return {false, 0, {}, "The request could not be encoded."};
    }
    static constexpr const char* MethodNames[] = {"GET", "POST", "PUT", "DELETE"};
    static constexpr HTTPC_RequestMethod HttpcMethods[] = {
        HTTPC_METHOD_GET, HTTPC_METHOD_POST, HTTPC_METHOD_PUT, HTTPC_METHOD_DELETE
    };
    const char* methodName = MethodNames[static_cast<int>(method)];

    if (path != "/v1/time") {
        syncClock();
    }
    const std::string timestamp = std::to_string(signedTimestampSeconds());
    const std::string signature = RequestSigning::sign(methodName, path, BuildConfig::Version, timestamp, body,
                                                       ServerConfig::clientSecret());

    Logger::instance().info(std::string(methodName) + " " + path);
    httpcContext context{};
    std::string openError;
    Result result = openTrustedContext(context, HttpcMethods[static_cast<int>(method)],
                                       ServerConfig::baseUrl() + path, openError);
    if (R_FAILED(result)) {
        Logger::instance().error("HTTP context setup failed: " + openError);
        return {false, 0, {}, openError};
    }

    const std::string version(BuildConfig::Version);
    const std::string authorization = accessToken.empty() ? std::string{} : "Bearer " + accessToken;
    const std::pair<const char*, const std::string*> headers[] = {
        {"Content-Type", body.empty() ? nullptr : &JsonContentType},
        {"Accept", &JsonContentType},
        {"X-Client-Version", &version},
        {"X-Client-Timestamp", &timestamp},
        {"X-Client-Signature", &signature},
        {"Authorization", authorization.empty() ? nullptr : &authorization},
    };
    for (const auto& [name, value] : headers) {
        if (value && R_SUCCEEDED(result)) {
            result = httpcAddRequestHeaderField(&context, name, value->c_str());
        }
    }
    std::vector<u32> upload((body.size() + sizeof(u32) - 1) / sizeof(u32));
    if (!body.empty() && R_SUCCEEDED(result)) {
        std::memcpy(upload.data(), body.data(), body.size());
        result = httpcAddPostDataRaw(&context, upload.data(), body.size());
    }
    const auto fail = [&](const char* stage, const std::string& message, bool sent) {
        const std::string code = resultCode(result);
        httpcCloseContext(&context);
        Logger::instance().error(std::string("HTTP ") + stage + " failed: " + code);
        return HttpResult{false, 0, {}, message + " (" + code + ").", sent};
    };
    if (R_FAILED(result)) {
        return fail("request setup", "Secure connection failed", false);
    }
    result = httpcBeginRequest(&context);
    if (R_FAILED(result)) {
        return fail("request", "Secure connection failed", true);
    }
    u32 status = 0;
    result = httpcGetResponseStatusCodeTimeout(&context, &status, timeoutNanoseconds);
    if (R_FAILED(result)) {
        return fail("response", "The server response failed", true);
    }

    std::uint32_t retryAfterSeconds = 0;
    if (status == 429) {
        char retryAfter[16]{};
        if (R_SUCCEEDED(httpcGetResponseHeader(&context, "Retry-After", retryAfter, sizeof(retryAfter) - 1))) {
            retryAfterSeconds = static_cast<std::uint32_t>(std::strtoul(retryAfter, nullptr, 10));
        }
    }

    u32 downloaded = 0;
    u32 contentSize = 0;
    httpcGetDownloadSizeState(&context, &downloaded, &contentSize);
    if (contentSize > MaximumResponseSize) {
        httpcCloseContext(&context);
        return {false, status, {}, "The server response is too large.", true, retryAfterSeconds};
    }
    std::vector<u8> response(MaximumResponseSize + 1);
    result = httpcReceiveData(&context, response.data(), MaximumResponseSize);
    httpcGetDownloadSizeState(&context, &downloaded, &contentSize);
    httpcCloseContext(&context);
    if (R_FAILED(result) && static_cast<u32>(result) != HTTPC_RESULTCODE_DOWNLOADPENDING) {
        const std::string code = resultCode(result);
        Logger::instance().error("HTTP download failed: " + code);
        return {false, status, {}, "The server response could not be downloaded (" + code + ").", true};
    }
    if (downloaded > MaximumResponseSize) {
        return {false, status, {}, "The server response is too large.", true};
    }

    Logger::instance().info("HTTP " + std::to_string(status));
    if (status == 401 && !accessToken.empty()) {
        sessionRejected_ = true;
    }
    return {true, status, std::string(reinterpret_cast<char*>(response.data()), downloaded), {}, true,
            retryAfterSeconds};
}
