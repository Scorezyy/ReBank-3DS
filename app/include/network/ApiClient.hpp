#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "network/DeviceIdentity.hpp"
#include "save/adapter/SaveAdapter.hpp"

struct AccountSession {
    std::string accountId;
    std::string accessToken;
    std::string refreshToken;
    std::uint16_t boxLimit = 50;
};

struct AuthResult {
    bool success = false;
    std::string message;
    AccountSession session;
    bool networkError = false;
    int httpStatus = 0;
};

struct UploadPokemon {
    std::uint16_t boxPosition = 1;
    std::uint8_t slot = 1;
    std::uint8_t format = 0;
    std::vector<std::uint8_t> payload;
    std::uint16_t species = 0;
    std::string nickname;
    std::string trainerName;
    std::uint8_t level = 0;
    std::string gameCode;
    bool shiny = false;
    std::uint16_t heldItem = 0;
};

struct UploadResult {
    bool success = false;
    std::string message;
    std::uint8_t storedCount = 0;
};

struct DownloadPokemon {
    std::uint16_t boxPosition = 1;
    std::uint8_t slot = 1;
    std::uint8_t format = 0;
    std::vector<std::uint8_t> payload;
    std::uint16_t species = 0;
    std::string nickname;
    std::string trainerName;
    std::uint8_t level = 0;
    std::string gameCode;
};

struct DownloadResult {
    bool success = false;
    std::string message;
    DownloadPokemon pokemon;
};

struct DeleteResult {
    bool success = false;
    std::string message;
};

struct RenameBoxResult {
    bool success = false;
    std::string message;
    std::string name;
};

struct BoxNameEntry {
    std::uint16_t position = 0;
    std::string name;
};

struct BoxNamesResult {
    bool success = false;
    std::string message;
    std::vector<BoxNameEntry> boxes;
};

struct BoxListResult {
    bool success = false;
    std::string message;
    std::array<PokemonSummary, 30> pokemon{};
    std::array<PokemonPayload, 30> payloads{};
};

struct ClientUpdate {
    bool success = false;
    std::string message;
    std::string tag;
    std::string version;
    std::string ciaSha256;
    std::string threeDsxSha256;
    std::uint32_t ciaSize = 0;
    std::uint32_t threeDsxSize = 0;
};

struct FileDownloadResult {
    bool success = false;
    std::string message;
    std::uint32_t size = 0;
};

class ApiClient {
public:
    ApiClient();
    ~ApiClient();
    bool available() const;
    AuthResult login(const std::string& username, const std::string& password);
    AuthResult registerAccount(
        const std::string& username,
        const std::string& email,
        const std::string& password
    );
    AuthResult refresh(const std::string& refreshToken);
    AuthResult requestPasswordReset(const std::string& email);
    UploadResult uploadPokemon(const std::vector<UploadPokemon>& pokemon, const std::string& accessToken);
    DownloadResult downloadPokemon(
        std::uint16_t boxPosition,
        std::uint8_t slot,
        const std::string& accessToken
    );
    DeleteResult deleteCloudPokemon(
        std::uint16_t boxPosition,
        std::uint8_t slot,
        const std::string& accessToken
    );
    BoxListResult listCloudBox(
        std::uint16_t boxPosition,
        const std::string& accessToken
    );
    RenameBoxResult renameBox(
        std::uint16_t boxPosition,
        const std::string& name,
        const std::string& accessToken
    );
    BoxNamesResult listBoxNames(const std::string& accessToken);
    ClientUpdate latestClientUpdate();
    FileDownloadResult downloadClientUpdate(
        const std::string& tag,
        const std::string& assetName,
        const std::string& destination,
        std::uint32_t expectedSize
    );

private:
    struct HttpResult {
        bool success = false;
        std::uint32_t status = 0;
        std::string body;
        std::string message;
    };

    AuthResult credentialsRequest(
        const char* path,
        const std::string& username,
        const std::string& password,
        const std::string& email = {},
        const std::string& deviceFingerprint = {}
    );
    AuthResult post(const char* path, const std::string& body);
    HttpResult request(
        const char* path,
        const std::string& body,
        const std::string& authorization = {},
        const char* method = nullptr
    );
    void syncClock();
    std::uint64_t signedTimestampSeconds();
    bool initialized_ = false;
    DeviceIdentity deviceIdentity_;
    std::optional<std::int64_t> clockDeltaMs_;
};