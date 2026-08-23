#include "ApiClient.hpp"

#include "Logger.hpp"
#include "ServerConfig.hpp"

#include <3ds.h>
#include <jansson.h>

#include <algorithm>
#include <cctype>
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

std::string encodeBase64(const std::vector<std::uint8_t>& input) {
    static constexpr char Alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);
    for (std::size_t index = 0; index < input.size(); index += 3) {
        const std::uint32_t first = input[index];
        const std::uint32_t second = index + 1 < input.size() ? input[index + 1] : 0;
        const std::uint32_t third = index + 2 < input.size() ? input[index + 2] : 0;
        const std::uint32_t value = (first << 16) | (second << 8) | third;
        output.push_back(Alphabet[(value >> 18) & 0x3F]);
        output.push_back(Alphabet[(value >> 12) & 0x3F]);
        output.push_back(index + 1 < input.size() ? Alphabet[(value >> 6) & 0x3F] : '=');
        output.push_back(index + 2 < input.size() ? Alphabet[value & 0x3F] : '=');
    }
    return output;
}

std::vector<std::uint8_t> decodeBase64(const std::string& input) {
    static constexpr int Decode[128] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    std::vector<std::uint8_t> output;
    output.reserve((input.size() * 3) / 4);
    std::uint32_t buffer = 0;
    int bits = 0;
    for (unsigned char c : input) {
        if (c == '=' || c >= 128) {
            break;
        }
        const int v = Decode[c];
        if (v < 0) {
            continue;
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<std::uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return output;
}

AuthResult parseResponse(u32 status, const std::string& response) {
    json_error_t error{};
    json_t* root = json_loadb(response.data(), response.size(), JSON_REJECT_DUPLICATES, &error);
    if (!root || !json_is_object(root)) {
        json_decref(root);
        return {false, "The server returned an invalid response.", {}};
    }

    if (status < 200 || status >= 300) {
        std::string message = stringField(root, "message");
        json_decref(root);
        if (message.empty()) {
            message = "The server rejected the request.";
        }
        return {false, std::move(message), {}};
    }

    if (status == 202) {
        json_decref(root);
        return {true, "Password reset request accepted.", {}};
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
        return {false, "The server returned an incomplete session.", {}};
    }
    return {true, "Authentication succeeded.", std::move(session)};
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
    return credentialsRequest("/v1/auth/register", username, password, email);
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
    const std::string& email
) {
    json_t* root = json_object();
    json_object_set_new(root, "username", json_string(username.c_str()));
    if (!email.empty()) {
        json_object_set_new(root, "email", json_string(email.c_str()));
    }
    json_object_set_new(root, "password", json_string(password.c_str()));
    const std::string body = dumpJson(root);
    json_decref(root);
    return post(path, body);
}

AuthResult ApiClient::post(const char* path, const std::string& body) {
    const HttpResult response = request(path, body);
    if (!response.success) {
        return {false, response.message, {}};
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
        json_t* entry = json_object();
        json_object_set_new(entry, "boxPosition", json_integer(item.boxPosition));
        json_object_set_new(entry, "slot", json_integer(item.slot));
        json_object_set_new(entry, "format", json_integer(item.format));
        json_object_set_new(entry, "payloadBase64", json_string(encodeBase64(item.payload).c_str()));
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
    json_error_t error{};
    json_t* parsed = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    if (!parsed || !json_is_object(parsed)) {
        json_decref(parsed);
        return {false, "The server returned an invalid upload response.", 0};
    }
    if (response.status < 200 || response.status >= 300) {
        std::string message = stringField(parsed, "message");
        Logger::instance().warning("Pokemon batch upload rejected (HTTP "
                                   + std::to_string(response.status) + "): " + message);
        json_decref(parsed);
        return {false, message.empty() ? "The upload was rejected." : std::move(message), 0};
    }
    json_t* stored = json_object_get(parsed, "stored");
    json_t* count = json_object_get(parsed, "count");
    const bool accepted = json_is_true(stored) && json_is_integer(count);
    const std::uint8_t storedCount = accepted
        ? static_cast<std::uint8_t>(json_integer_value(count))
        : static_cast<std::uint8_t>(0);
    json_decref(parsed);
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
    json_error_t error{};
    json_t* parsed = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    if (!parsed || !json_is_object(parsed)) {
        json_decref(parsed);
        return {false, "The server returned an invalid response.", {}};
    }
    if (response.status < 200 || response.status >= 300) {
        std::string message = stringField(parsed, "message");
        json_decref(parsed);
        return {false, message.empty() ? "Download rejected." : std::move(message), {}};
    }

    DownloadPokemon mon;
    json_t* value = nullptr;
    value = json_object_get(parsed, "boxPosition");
    mon.boxPosition = static_cast<std::uint16_t>(json_is_integer(value) ? json_integer_value(value) : boxPosition);
    value = json_object_get(parsed, "slot");
    mon.slot = static_cast<std::uint8_t>(json_is_integer(value) ? json_integer_value(value) : slot);
    value = json_object_get(parsed, "format");
    mon.format = static_cast<std::uint8_t>(json_is_integer(value) ? json_integer_value(value) : 0);
    value = json_object_get(parsed, "species");
    mon.species = static_cast<std::uint16_t>(json_is_integer(value) ? json_integer_value(value) : 0);
    value = json_object_get(parsed, "level");
    mon.level = static_cast<std::uint8_t>(json_is_integer(value) ? json_integer_value(value) : 0);
    mon.nickname = stringField(parsed, "nickname");
    mon.trainerName = stringField(parsed, "trainerName");
    mon.gameCode = stringField(parsed, "gameCode");
    mon.payload = decodeBase64(stringField(parsed, "payloadBase64"));
    json_decref(parsed);
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
        json_error_t error{};
        json_t* parsed = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
        std::string message = parsed ? stringField(parsed, "message") : std::string{};
        json_decref(parsed);
        return {false, message.empty() ? "Delete rejected." : std::move(message)};
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
    json_error_t error{};
    json_t* parsed = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    if (!parsed || !json_is_object(parsed)) {
        json_decref(parsed);
        result.message = "The server returned an invalid response.";
        return result;
    }
    if (response.status < 200 || response.status >= 300) {
        std::string message = stringField(parsed, "message");
        json_decref(parsed);
        result.message = message.empty() ? "Box listing rejected." : std::move(message);
        return result;
    }
    json_t* array = json_object_get(parsed, "pokemon");
    if (json_is_array(array)) {
        const std::size_t count = json_array_size(array);
        for (std::size_t i = 0; i < count; ++i) {
            json_t* entry = json_array_get(array, i);
            if (!json_is_object(entry)) {
                continue;
            }
            json_t* slotValue = json_object_get(entry, "slot");
            const int slot = json_is_integer(slotValue) ? static_cast<int>(json_integer_value(slotValue)) : 0;
            if (slot < 1 || slot > 30) {
                continue;
            }
            PokemonSummary summary;
            json_t* species = json_object_get(entry, "species");
            summary.species = static_cast<std::uint16_t>(json_is_integer(species) ? json_integer_value(species) : 0);
            json_t* level = json_object_get(entry, "level");
            summary.level = static_cast<std::uint8_t>(json_is_integer(level) ? json_integer_value(level) : 0);
            summary.nickname = stringField(entry, "nickname");
            summary.trainerName = stringField(entry, "trainerName");
            summary.gameCode = stringField(entry, "gameCode");
            json_t* formatValue = json_object_get(entry, "format");
            summary.format = static_cast<std::uint8_t>(json_is_integer(formatValue) ? json_integer_value(formatValue) : 0);
            json_t* shinyValue = json_object_get(entry, "shiny");
            summary.shiny = json_is_true(shinyValue);
            json_t* heldItemValue = json_object_get(entry, "heldItem");
            summary.heldItem = static_cast<std::uint16_t>(
                json_is_integer(heldItemValue) ? json_integer_value(heldItemValue) : 0);
            result.pokemon[static_cast<std::size_t>(slot - 1)] = std::move(summary);
        }
    }
    json_decref(parsed);
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
    json_error_t error{};
    json_t* parsed = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    if (!parsed || !json_is_object(parsed)) {
        json_decref(parsed);
        return {false, "The server returned an invalid response.", {}};
    }
    if (response.status < 200 || response.status >= 300) {
        std::string message = stringField(parsed, "message");
        json_decref(parsed);
        return {false, message.empty() ? "Rename rejected." : std::move(message), {}};
    }
    std::string storedName = stringField(parsed, "name");
    json_decref(parsed);
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
    json_error_t error{};
    json_t* parsed = json_loadb(response.body.data(), response.body.size(), JSON_REJECT_DUPLICATES, &error);
    if (!parsed || !json_is_object(parsed)) {
        json_decref(parsed);
        return {false, "The server returned an invalid response.", {}};
    }
    if (response.status < 200 || response.status >= 300) {
        std::string message = stringField(parsed, "message");
        json_decref(parsed);
        return {false, message.empty() ? "Box listing rejected." : std::move(message), {}};
    }
    std::vector<BoxNameEntry> boxes;
    json_t* array = json_object_get(parsed, "boxes");
    if (json_is_array(array)) {
        const std::size_t count = json_array_size(array);
        for (std::size_t i = 0; i < count; ++i) {
            json_t* entry = json_array_get(array, i);
            if (!json_is_object(entry)) {
                continue;
            }
            json_t* positionValue = json_object_get(entry, "position");
            if (!json_is_integer(positionValue)) {
                continue;
            }
            BoxNameEntry item;
            item.position = static_cast<std::uint16_t>(json_integer_value(positionValue));
            item.name = stringField(entry, "name");
            if (!item.name.empty()) {
                boxes.push_back(std::move(item));
            }
        }
    }
    json_decref(parsed);
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
    Result result = httpcOpenContext(&context, HTTPC_METHOD_GET, url.c_str(), 0);
    const auto& rootCertificate = trustedRootCertificate();
    if (R_SUCCEEDED(result) && rootCertificate.empty()) {
        httpcCloseContext(&context);
        return {false, "The trusted server certificate is unavailable.", 0};
    }
    if (R_SUCCEEDED(result)) {
        result = httpcAddTrustedRootCA(
            &context,
            rootCertificate.data(),
            static_cast<u32>(rootCertificate.size())
        );
    }
    if (R_SUCCEEDED(result)) {
        result = httpcAddRequestHeaderField(&context, "Accept", "application/octet-stream");
    }
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

    const std::string url = ServerConfig::baseUrl() + path;
    Logger::instance().info(std::string(
        httpMethod == HTTPC_METHOD_GET ? "GET " :
        httpMethod == HTTPC_METHOD_DELETE ? "DELETE " :
        httpMethod == HTTPC_METHOD_PUT ? "PUT " : "POST "
    ) + path);
    httpcContext context{};
    Result result = httpcOpenContext(&context, httpMethod, url.c_str(), 0);
    if (R_FAILED(result)) {
        const std::string code = resultCode(result);
        Logger::instance().error("HTTP context failed: " + code);
        return {false, 0, {}, "Connection setup failed (" + code + ")."};
    }

    const auto& rootCertificate = trustedRootCertificate();
    if (rootCertificate.empty()) {
        httpcCloseContext(&context);
        Logger::instance().error("Trusted server certificate is unavailable");
        return {false, 0, {}, "The trusted server certificate is unavailable."};
    }
    result = httpcAddTrustedRootCA(
        &context,
        rootCertificate.data(),
        static_cast<u32>(rootCertificate.size())
    );
    if (R_FAILED(result)) {
        const std::string code = resultCode(result);
        httpcCloseContext(&context);
        Logger::instance().error("Trusted root CA failed: " + code);
        return {false, 0, {}, "TLS certificate setup failed (" + code + ")."};
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