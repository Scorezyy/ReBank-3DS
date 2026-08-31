#include "network/ApiClient.hpp"

#include "BuildConfig.hpp"
#include "core/Base64.hpp"
#include "core/Logger.hpp"
#include "core/RequestSigning.hpp"
#include "core/ServerConfig.hpp"
#include "save/pokemon/PokemonTransfer.hpp"

#include <3ds.h>
#include <jansson.h>
#include <pkx/PKX.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr std::size_t MaximumResponseSize = 64 * 1024;
constexpr std::size_t MaximumCertificateSize = 16 * 1024;
constexpr std::size_t MaximumUpdateSize = 5 * 1024 * 1024;
constexpr std::size_t DownloadChunkSize = 32 * 1024;
constexpr u64 RequestTimeout = 30'000'000'000ULL;

std::string resultCode(Result result) {
    char text[11]{};
    std::snprintf(text, sizeof(text), "0x%08lX", static_cast<unsigned long>(result));
    return text;
}

const std::vector<u8>& trustedRootCertificate() {
    static const std::vector<u8> certificate = [] {
        FILE* file = std::fopen("romfs:/assets/rebank-ca.der", "rb");
        if (!file) {
            return std::vector<u8>{};
        }
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::rewind(file);
        if (size <= 0 || static_cast<std::size_t>(size) > MaximumCertificateSize) {
            std::fclose(file);
            return std::vector<u8>{};
        }
        std::vector<u8> contents(static_cast<std::size_t>(size));
        const std::size_t read = std::fread(contents.data(), 1, contents.size(), file);
        std::fclose(file);
        if (read != contents.size()) {
            return std::vector<u8>{};
        }
        return contents;
    }();
    return certificate;
}

std::string dumpJson(json_t* value) {
    char* encoded = json_dumps(value, JSON_COMPACT);
    if (!encoded) {
        return {};
    }
    std::string result(encoded);
    std::free(encoded);
    return result;
}

std::string stringField(json_t* object, const char* key) {
    json_t* value = json_object_get(object, key);
    return json_is_string(value) ? json_string_value(value) : "";
}

template <typename T>
T intField(json_t* object, const char* key, T fallback) {
    json_t* value = json_object_get(object, key);
    return json_is_integer(value) ? static_cast<T>(json_integer_value(value)) : fallback;
}

bool boolField(json_t* object, const char* key) {
    return json_is_true(json_object_get(object, key));
}

Result openTrustedContext(httpcContext& context, HTTPC_RequestMethod method, const std::string& url, std::string& outMessage) {
    Result result = httpcOpenContext(&context, method, url.c_str(), 0);
    if (R_FAILED(result)) {
        outMessage = "Connection setup failed (" + resultCode(result) + ").";
        return result;
    }
    const auto& rootCertificate = trustedRootCertificate();
    if (rootCertificate.empty()) {
        httpcCloseContext(&context);
        outMessage = "The trusted server certificate is unavailable.";
        return static_cast<Result>(-1);
    }
    result = httpcAddTrustedRootCA(&context, rootCertificate.data(), static_cast<u32>(rootCertificate.size()));
    if (R_FAILED(result)) {
        httpcCloseContext(&context);
        outMessage = "TLS certificate setup failed (" + resultCode(result) + ").";
    }
    return result;
}

bool validUpdateText(const std::string& value, bool tag) {
    if (value.empty() || value.size() > 48 || (tag && value[0] != 'v')) {
        return false;
    }
    return std::all_of(value.begin() + (tag ? 1 : 0), value.end(), [](unsigned char character) {
        return std::isdigit(character) || character == '.' || character == '-'
            || (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
    });
}

bool validSha256(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isdigit(character) || (character >= 'a' && character <= 'f');
    });
}

struct JsonEnvelope {
    json_t* root = nullptr;
    bool ok = false;
    std::string message;
};

JsonEnvelope parseEnvelope(u32 status, const std::string& body, const char* rejectionFallback) {
    json_error_t error{};
    json_t* root = json_loadb(body.data(), body.size(), JSON_REJECT_DUPLICATES, &error);
    if (!root || !json_is_object(root)) {
        json_decref(root);
        return {nullptr, false, "The server returned an invalid response."};
    }
    if (status < 200 || status >= 300) {
        std::string message = stringField(root, "message");
        json_decref(root);
        return {nullptr, false, message.empty() ? rejectionFallback : std::move(message)};
    }
    return {root, true, {}};
}

bool parsePokemonSummaryEntry(
    json_t* entry,
    PokemonSummary& summary,
    int& slot,
    std::vector<std::uint8_t>& payload
) {
    if (!json_is_object(entry)) {
        return false;
    }
    slot = intField<int>(entry, "slot", 0);
    if (slot < 1 || slot > 30) {
        return false;
    }

    summary.species = intField<std::uint16_t>(entry, "species", 0);
    summary.level = intField<std::uint8_t>(entry, "level", 0);
    summary.nickname = stringField(entry, "nickname");
    summary.trainerName = stringField(entry, "trainerName");
    summary.gameCode = stringField(entry, "gameCode");
    summary.format = intField<std::uint8_t>(entry, "format", 0);
    summary.shiny = boolField(entry, "shiny");
    summary.heldItem = intField<std::uint16_t>(entry, "heldItem", 0);

    payload = Base64::decode(stringField(entry, "payloadBase64"));
    const pksm::Generation gen = PokemonTransfer::generationFromFormat(summary.format);
    if (payload.empty() || gen == pksm::Generation::UNUSED) {
        return true;
    }
    std::vector<std::uint8_t> buffer(payload);
    auto pkx = pksm::PKX::getPKM(gen, buffer.data(), buffer.size(), false);
    if (!pkx || static_cast<std::uint16_t>(pkx->species()) == 0) {
        return true;
    }
    summary.type1 = pkx->type1();
    summary.type2 = pkx->type2();
    summary.originGame = pkx->version();
    summary.language = pkx->language();
    summary.moves = {pkx->move(0), pkx->move(1), pkx->move(2), pkx->move(3)};
    summary.ability = pkx->ability();
    summary.nature = pkx->nature();
    summary.gender = pkx->gender();
    return true;
}

AuthResult parseResponse(u32 status, const std::string& response) {
    const int httpStatus = static_cast<int>(status);
    json_error_t error{};
    json_t* root = json_loadb(response.data(), response.size(), JSON_REJECT_DUPLICATES, &error);
    if (!root || !json_is_object(root)) {
        json_decref(root);
        return {false, "The server returned an invalid response.", {}, false, httpStatus};
    }

    if (status < 200 || status >= 300) {
        std::string message = stringField(root, "message");
        json_decref(root);
        if (message.empty()) {
            message = "The server rejected the request.";
        }
        return {false, std::move(message), {}, false, httpStatus};
    }

    if (status == 202) {
        json_decref(root);
        return {true, "Password reset request accepted.", {}, false, httpStatus};
    }

    json_t* account = json_object_get(root, "account");
    json_t* boxLimit = account ? json_object_get(account, "boxLimit") : nullptr;
    AccountSession session{
        account ? stringField(account, "id") : "",
        stringField(root, "accessToken"),
        stringField(root, "refreshToken"),
        static_cast<std::uint16_t>(json_is_integer(boxLimit) ? json_integer_value(boxLimit) : 50)
    };
    json_decref(root);
    if (session.accountId.empty() || session.accessToken.empty() || session.refreshToken.empty()) {
        return {false, "The server returned an incomplete session.", {}, false, httpStatus};
    }
    return {true, "Authentication succeeded.", std::move(session), false, httpStatus};
}
}

ApiClient::ApiClient() {
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

bool ApiClient::available() const {
    return initialized_;
}

AuthResult ApiClient::login(const std::string& username, const std::string& password) {
    return credentialsRequest("/v1/auth/login", username, password);
}

AuthResult ApiClient::registerAccount(
    const std::string& username,
    const std::string& email,
    const std::string& password
) {
    const std::string deviceFingerprint = deviceIdentity_.fingerprint();
    if (deviceFingerprint.empty()) {
        return {false, "This console's identity could not be verified. Registration requires a genuine, unmodified 3DS.", {}, false, 0};
    }
    return credentialsRequest("/v1/auth/register", username, password, email, deviceFingerprint);
}

AuthResult ApiClient::refresh(const std::string& refreshToken) {
    json_t* root = json_object();
    json_object_set_new(root, "refreshToken", json_string(refreshToken.c_str()));
    const std::string body = dumpJson(root);
    json_decref(root);
    return post("/v1/auth/refresh", body);
}

AuthResult ApiClient::requestPasswordReset(const std::string& email) {
    json_t* root = json_object();
    json_object_set_new(root, "email", json_string(email.c_str()));
    const std::string body = dumpJson(root);
    json_decref(root);
    return post("/v1/auth/password-reset", body);
}

AuthResult ApiClient::credentialsRequest(
    const char* path,
    const std::string& username,
    const std::string& password,
    const std::string& email,
    const std::string& deviceFingerprint
) {
    json_t* root = json_object();
    json_object_set_new(root, "username", json_string(username.c_str()));
    if (!email.empty()) {
        json_object_set_new(root, "email", json_string(email.c_str()));
    }
    json_object_set_new(root, "password", json_string(password.c_str()));
    if (!deviceFingerprint.empty()) {
        json_object_set_new(root, "deviceFingerprint", json_string(deviceFingerprint.c_str()));
    }
    const std::string body = dumpJson(root);
    json_decref(root);
    return post(path, body);
}

AuthResult ApiClient::post(const char* path, const std::string& body) {
    const HttpResult response = request(path, body);
    if (!response.success) {
        return {false, response.message, {}, true};
    }
    return parseResponse(response.status, response.body);
}

UploadResult ApiClient::uploadPokemon(
    const std::vector<UploadPokemon>& pokemon,
    const std::string& accessToken
) {
    if (pokemon.empty() || pokemon.size() > 30 || accessToken.empty()) {
        return {false, "The upload request is incomplete.", 0};
    }
    json_t* root = json_object();
    json_t* entries = json_array();
    for (const auto& item : pokemon) {
        const std::string nickname = item.nickname.empty() ? "Pokemon" : item.nickname;
        const std::string trainerName = item.trainerName.empty() ? "Unknown" : item.trainerName;
        const std::string gameCode = item.gameCode.empty() ? "unknown" : item.gameCode;
        Logger::instance().info("Uploading box " + std::to_string(item.boxPosition) + " slot "
                                + std::to_string(item.slot) + " species " + std::to_string(item.species)
                                + " format " + std::to_string(item.format) + " level " + std::to_string(item.level)
                                + " nickname \"" + nickname + "\" (" + std::to_string(nickname.size()) + " chars)"
                                + " trainerName \"" + trainerName + "\" (" + std::to_string(trainerName.size()) + " chars)"
                                + " gameCode \"" + gameCode + "\""
                                + " heldItem " + std::to_string(item.heldItem)
                                + " payloadBytes " + std::to_string(item.payload.size()));
        json_t* entry = json_object();
        json_object_set_new(entry, "boxPosition", json_integer(item.boxPosition));
        json_object_set_new(entry, "slot", json_integer(item.slot));
        json_object_set_new(entry, "format", json_integer(item.format));
        json_object_set_new(entry, "payloadBase64", json_string(Base64::encode(item.payload).c_str()));
        json_object_set_new(entry, "species", json_integer(item.species));
        json_object_set_new(entry, "nickname", json_string(nickname.c_str()));
        json_object_set_new(entry, "trainerName", json_string(trainerName.c_str()));
        json_object_set_new(entry, "level", json_integer(item.level));
        json_object_set_new(entry, "gameCode", json_string(gameCode.c_str()));
        json_object_set_new(entry, "shiny", json_boolean(item.shiny));
        json_object_set_new(entry, "heldItem", json_integer(item.heldItem));
        json_array_append_new(entries, entry);
    }
    json_object_set_new(root, "pokemon", entries);
    const std::string body = dumpJson(root);
    json_decref(root);

    const HttpResult response = request("/v1/pokemon/batch", body, "Bearer " + accessToken);
    if (!response.success) {
        return {false, response.message, 0};
    }
    JsonEnvelope envelope = parseEnvelope(response.status, response.body, "The upload was rejected.");
    if (!envelope.ok) {
        Logger::instance().warning("Pokemon batch upload rejected (HTTP "
                                   + std::to_string(response.status) + "): " + envelope.message);
        return {false, std::move(envelope.message), 0};
    }
    const bool accepted = boolField(envelope.root, "stored") && json_is_integer(json_object_get(envelope.root, "count"));
    const std::uint8_t storedCount = intField<std::uint8_t>(envelope.root, "count", 0);
    json_decref(envelope.root);
    if (!accepted) {
        return {false, "The server returned an incomplete upload response.", 0};
    }
    return {true, "Upload complete.", storedCount};
}

DownloadResult ApiClient::downloadPokemon(
    std::uint16_t boxPosition,
    std::uint8_t slot,
    const std::string& accessToken
) {
    if (accessToken.empty()) {
        return {false, "Not signed in.", {}};
    }
    const std::string path = "/v1/pokemon/" + std::to_string(boxPosition) + "/" + std::to_string(slot);
    const HttpResult response = request(path.c_str(), {}, "Bearer " + accessToken, "GET");
    if (!response.success) {
        return {false, response.message, {}};
    }
    JsonEnvelope envelope = parseEnvelope(response.status, response.body, "Download rejected.");
    if (!envelope.ok) {
        return {false, std::move(envelope.message), {}};
    }

    DownloadPokemon mon;
    mon.boxPosition = intField<std::uint16_t>(envelope.root, "boxPosition", boxPosition);
    mon.slot = intField<std::uint8_t>(envelope.root, "slot", slot);
    mon.format = intField<std::uint8_t>(envelope.root, "format", 0);
    mon.species = intField<std::uint16_t>(envelope.root, "species", 0);
    mon.level = intField<std::uint8_t>(envelope.root, "level", 0);
    mon.nickname = stringField(envelope.root, "nickname");
    mon.trainerName = stringField(envelope.root, "trainerName");
    mon.gameCode = stringField(envelope.root, "gameCode");
    mon.payload = Base64::decode(stringField(envelope.root, "payloadBase64"));
    json_decref(envelope.root);
    if (mon.payload.empty() || mon.format == 0) {
        return {false, "Payload missing.", {}};
    }
    return {true, "Download complete.", std::move(mon)};
}

DeleteResult ApiClient::deleteCloudPokemon(
    std::uint16_t boxPosition,
    std::uint8_t slot,
    const std::string& accessToken
) {
    if (accessToken.empty()) {
        return {false, "Not signed in."};
    }
    const std::string path = "/v1/pokemon/" + std::to_string(boxPosition) + "/" + std::to_string(slot);
    const HttpResult response = request(path.c_str(), {}, "Bearer " + accessToken, "DELETE");
    if (!response.success) {
        return {false, response.message};
    }
    if (response.status < 200 || response.status >= 300) {
        JsonEnvelope envelope = parseEnvelope(response.status, response.body, "Delete rejected.");
        json_decref(envelope.root);
        return {false, envelope.ok ? "Delete rejected." : std::move(envelope.message)};
    }
    return {true, "Deleted."};
}

BoxListResult ApiClient::listCloudBox(
    std::uint16_t boxPosition,
    const std::string& accessToken
) {
    BoxListResult result;
    if (accessToken.empty()) {
        result.message = "Not signed in.";
        return result;
    }
    const std::string path = "/v1/boxes/" + std::to_string(boxPosition);
    const HttpResult response = request(path.c_str(), {}, "Bearer " + accessToken, "GET");
    if (!response.success) {
        result.message = response.message;
        return result;
    }
    JsonEnvelope envelope = parseEnvelope(response.status, response.body, "Box listing rejected.");
    if (!envelope.ok) {
        result.message = std::move(envelope.message);
        return result;
    }
    json_t* array = json_object_get(envelope.root, "pokemon");
    if (json_is_array(array)) {
        const std::size_t count = json_array_size(array);
        for (std::size_t i = 0; i < count; ++i) {
            PokemonSummary summary;
            int slot = 0;
            std::vector<std::uint8_t> payload;
            if (!parsePokemonSummaryEntry(json_array_get(array, i), summary, slot, payload)) {
                continue;
            }
            if (!payload.empty()) {
                result.payloads[static_cast<std::size_t>(slot - 1)] = {summary.format, payload};
            }
            result.pokemon[static_cast<std::size_t>(slot - 1)] = std::move(summary);
        }
    }
    json_decref(envelope.root);
    result.success = true;
    result.message = "Box loaded.";
    return result;
}

RenameBoxResult ApiClient::renameBox(
    std::uint16_t boxPosition,
    const std::string& name,
    const std::string& accessToken
) {
    if (accessToken.empty()) {
        return {false, "Not signed in.", {}};
    }
    if (name.empty()) {
        return {false, "The box name cannot be blank.", {}};
    }
    json_t* root = json_object();
    json_object_set_new(root, "name", json_string(name.c_str()));
    const std::string body = dumpJson(root);
    json_decref(root);

    const std::string path = "/v1/boxes/" + std::to_string(boxPosition) + "/name";
    const HttpResult response = request(path.c_str(), body, "Bearer " + accessToken, "PUT");
    if (!response.success) {
        return {false, response.message, {}};
    }
    JsonEnvelope envelope = parseEnvelope(response.status, response.body, "Rename rejected.");
    if (!envelope.ok) {
        return {false, std::move(envelope.message), {}};
    }
    std::string storedName = stringField(envelope.root, "name");
    json_decref(envelope.root);
    return {true, "Box renamed.", storedName.empty() ? name : std::move(storedName)};
}

BoxNamesResult ApiClient::listBoxNames(const std::string& accessToken) {
    if (accessToken.empty()) {
        return {false, "Not signed in.", {}};
    }
    const HttpResult response = request("/v1/boxes", {}, "Bearer " + accessToken, "GET");
    if (!response.success) {
        return {false, response.message, {}};
    }
    JsonEnvelope envelope = parseEnvelope(response.status, response.body, "Box listing rejected.");
    if (!envelope.ok) {
        return {false, std::move(envelope.message), {}};
    }
    std::vector<BoxNameEntry> boxes;
    json_t* array = json_object_get(envelope.root, "boxes");
    if (json_is_array(array)) {
        const std::size_t count = json_array_size(array);
        for (std::size_t i = 0; i < count; ++i) {
            json_t* entry = json_array_get(array, i);
            if (!json_is_object(entry) || !json_is_integer(json_object_get(entry, "position"))) {
                continue;
            }
            BoxNameEntry item;
            item.position = intField<std::uint16_t>(entry, "position", 0);
            item.name = stringField(entry, "name");
            if (!item.name.empty()) {
                boxes.push_back(std::move(item));
            }
        }
    }
    json_decref(envelope.root);
    return {true, "Boxes loaded.", std::move(boxes)};
}

ClientUpdate ApiClient::latestClientUpdate() {
    const HttpResult response = request("/v1/client/update", {}, {}, "GET");
    if (!response.success) {
        return {false, response.message};
    }
    json_error_t error{};
    json_t* root = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    json_t* assets = root ? json_object_get(root, "assets") : nullptr;
    if (response.status != 200 || !json_is_object(root) || !json_is_array(assets)) {
        json_decref(root);
        return {false, "The server returned invalid update information."};
    }
    ClientUpdate update{};
    update.tag = stringField(root, "tag");
    update.version = stringField(root, "version");
    const std::size_t count = json_array_size(assets);
    for (std::size_t index = 0; index < count; ++index) {
        json_t* asset = json_array_get(assets, index);
        const std::string name = json_is_object(asset) ? stringField(asset, "name") : "";
        const std::string digest = json_is_object(asset) ? stringField(asset, "sha256") : "";
        json_t* sizeValue = json_is_object(asset) ? json_object_get(asset, "size") : nullptr;
        const json_int_t size = json_is_integer(sizeValue) ? json_integer_value(sizeValue) : 0;
        if (!validSha256(digest) || size <= 0 || size > static_cast<json_int_t>(MaximumUpdateSize)) {
            continue;
        }
        if (name == "ReBank.cia") {
            update.ciaSha256 = digest;
            update.ciaSize = static_cast<std::uint32_t>(size);
        } else if (name == "ReBank.3dsx") {
            update.threeDsxSha256 = digest;
            update.threeDsxSize = static_cast<std::uint32_t>(size);
        }
    }
    json_decref(root);
    if (!validUpdateText(update.tag, true) || !validUpdateText(update.version, false)
        || update.ciaSha256.empty() || update.threeDsxSha256.empty()) {
        return {false, "The server returned incomplete update information."};
    }
    update.success = true;
    update.message = "Update information loaded.";
    return update;
}

FileDownloadResult ApiClient::downloadClientUpdate(
    const std::string& tag,
    const std::string& assetName,
    const std::string& destination,
    std::uint32_t expectedSize
) {
    if (!initialized_ || !validUpdateText(tag, true)
        || (assetName != "ReBank.cia" && assetName != "ReBank.3dsx")
        || expectedSize == 0 || expectedSize > MaximumUpdateSize) {
        return {false, "The update download request is invalid.", 0};
    }
    const std::string path = "/v1/client/update/" + tag + "/" + assetName;
    const std::string url = ServerConfig::baseUrl() + path;
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
        result = httpcGetResponseStatusCodeTimeout(&context, &status, RequestTimeout);
    }
    if (R_FAILED(result) || status != 200) {
        const std::string code = resultCode(result);
        httpcCloseContext(&context);
        return {false, "Update connection failed (" + code + ").", 0};
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
        u32 contentSize = 0;
        httpcGetDownloadSizeState(&context, &before, &contentSize);
        result = httpcReceiveDataTimeout(&context, buffer.data(), buffer.size(), RequestTimeout);
        u32 after = 0;
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
    if (clockDeltaMs_.has_value()) {
        return;
    }
    const std::int64_t localMsBeforeSync = static_cast<std::int64_t>(osGetTime());
    const HttpResult response = request("/v1/time", {}, {}, "GET");
    if (!response.success || response.status != 200) {
        return;
    }
    json_error_t error{};
    json_t* root = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    json_t* unixSecondsField = root ? json_object_get(root, "unixSeconds") : nullptr;
    if (!json_is_integer(unixSecondsField)) {
        json_decref(root);
        return;
    }
    const std::int64_t serverUnixSeconds = static_cast<std::int64_t>(json_integer_value(unixSecondsField));
    json_decref(root);
    clockDeltaMs_ = serverUnixSeconds * 1000 - localMsBeforeSync;
    Logger::instance().info("Clock synced with server, delta=" + std::to_string(*clockDeltaMs_) + "ms");
}

std::uint64_t ApiClient::signedTimestampSeconds() {
    const std::int64_t localMs = static_cast<std::int64_t>(osGetTime());
    if (clockDeltaMs_.has_value()) {
        
        return static_cast<std::uint64_t>((localMs + *clockDeltaMs_) / 1000);
    }

    constexpr std::uint64_t NtpToUnixEpochOffsetSeconds = 2208988800ULL;
    std::int64_t timeOffsetMs = 0;
    CFGU_GetConfigInfoBlk2(sizeof(timeOffsetMs), 0x00030001, &timeOffsetMs);
    const std::int64_t correctedMs = localMs - timeOffsetMs;
    return static_cast<std::uint64_t>(correctedMs) / 1000ULL - NtpToUnixEpochOffsetSeconds;
}

ApiClient::HttpResult ApiClient::request(
    const char* path,
    const std::string& body,
    const std::string& authorization,
    const char* method
) {
    if (!initialized_) {
        return {false, 0, {}, "The network service is unavailable."};
    }
    const bool hasBody = !body.empty();
    if (hasBody && body.size() > 0xF000) {
        return {false, 0, {}, "The request could not be encoded."};
    }

    HTTPC_RequestMethod httpMethod = HTTPC_METHOD_POST;
    if (method) {
        if (std::strcmp(method, "GET") == 0) {
            httpMethod = HTTPC_METHOD_GET;
        } else if (std::strcmp(method, "DELETE") == 0) {
            httpMethod = HTTPC_METHOD_DELETE;
        } else if (std::strcmp(method, "PUT") == 0) {
            httpMethod = HTTPC_METHOD_PUT;
        }
    }
    const char* methodName = method ? method : "POST";

    if (std::strcmp(path, "/v1/time") != 0) {
        syncClock();
    }
    const std::string timestampText = std::to_string(signedTimestampSeconds());
    const std::string signature = RequestSigning::sign(
        methodName, path, BuildConfig::Version, timestampText,
        hasBody ? body : std::string{}, ServerConfig::clientSecret()
    );

    const std::string url = ServerConfig::baseUrl() + path;
    Logger::instance().info(std::string(
        httpMethod == HTTPC_METHOD_GET ? "GET " :
        httpMethod == HTTPC_METHOD_DELETE ? "DELETE " :
        httpMethod == HTTPC_METHOD_PUT ? "PUT " : "POST "
    ) + path);
    httpcContext context{};
    std::string openError;
    Result result = openTrustedContext(context, httpMethod, url, openError);
    if (R_FAILED(result)) {
        Logger::instance().error("HTTP context setup failed: " + openError);
        return {false, 0, {}, openError};
    }

    std::vector<u32> upload;
    if (hasBody) {
        upload.resize((body.size() + sizeof(u32) - 1) / sizeof(u32));
        std::memcpy(upload.data(), body.data(), body.size());
    }
    if (hasBody) {
        result = httpcAddRequestHeaderField(&context, "Content-Type", "application/json");
    }
    if (R_SUCCEEDED(result)) {
        result = httpcAddRequestHeaderField(&context, "Accept", "application/json");
    }
    if (R_SUCCEEDED(result)) {
        result = httpcAddRequestHeaderField(&context, "X-Client-Version", std::string(BuildConfig::Version).c_str());
    }
    if (R_SUCCEEDED(result)) {
        result = httpcAddRequestHeaderField(&context, "X-Client-Timestamp", timestampText.c_str());
    }
    if (R_SUCCEEDED(result)) {
        result = httpcAddRequestHeaderField(&context, "X-Client-Signature", signature.c_str());
    }
    if (R_SUCCEEDED(result) && !authorization.empty()) {
        result = httpcAddRequestHeaderField(&context, "Authorization", authorization.c_str());
    }
    if (R_SUCCEEDED(result) && hasBody) {
        result = httpcAddPostDataRaw(&context, upload.data(), body.size());
    }
    if (R_SUCCEEDED(result)) {
        result = httpcBeginRequest(&context);
    }
    if (R_FAILED(result)) {
        const std::string code = resultCode(result);
        httpcCloseContext(&context);
        Logger::instance().error("HTTP request setup failed: " + code);
        return {false, 0, {}, "Secure connection failed (" + code + ")."};
    }

    u32 status = 0;
    result = httpcGetResponseStatusCodeTimeout(&context, &status, RequestTimeout);
    if (R_FAILED(result)) {
        const std::string code = resultCode(result);
        httpcCloseContext(&context);
        Logger::instance().error("HTTP response failed: " + code);
        return {false, 0, {}, "The server response failed (" + code + ")."};
    }

    u32 downloaded = 0;
    u32 contentSize = 0;
    httpcGetDownloadSizeState(&context, &downloaded, &contentSize);
    if (contentSize > MaximumResponseSize) {
        httpcCloseContext(&context);
        return {false, 0, {}, "The server response is too large."};
    }

    std::vector<u8> response(MaximumResponseSize + 1);
    result = httpcReceiveData(&context, response.data(), MaximumResponseSize);
    httpcGetDownloadSizeState(&context, &downloaded, &contentSize);
    httpcCloseContext(&context);
    if (R_FAILED(result) && static_cast<u32>(result) != HTTPC_RESULTCODE_DOWNLOADPENDING) {
        const std::string code = resultCode(result);
        Logger::instance().error("HTTP download failed: " + code);
        return {false, 0, {}, "The server response could not be downloaded (" + code + ")."};
    }
    if (downloaded > MaximumResponseSize) {
        return {false, 0, {}, "The server response is too large."};
    }

    response[downloaded] = 0;
    Logger::instance().info("HTTP " + std::to_string(status));
    return {
        true,
        status,
        std::string(reinterpret_cast<char*>(response.data()), downloaded),
        {}
    };
}