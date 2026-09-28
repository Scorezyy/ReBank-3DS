#pragma once

#include "save/pokemon/PokemonData.hpp"

#include <3ds.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

enum class RequestOutcome : std::uint8_t {
    Succeeded,
    Refused,
    Unknown
};

struct CloudSlot {
    std::uint16_t boxPosition = 0;
    std::uint8_t slot = 0;
};

struct UploadPokemon {
    CloudSlot destination;
    PokemonSummary summary;
    PokemonPayload payload;
    bool replaceOccupant = true;
};

struct UploadRejection {
    CloudSlot slot;
    std::string reason;
};

struct UploadResult {
    RequestOutcome outcome = RequestOutcome::Unknown;
    std::string message;
    std::vector<UploadRejection> rejected;
};

struct MoveResult {
    RequestOutcome outcome = RequestOutcome::Unknown;
    std::string message;
};

struct DeleteResult {
    RequestOutcome outcome = RequestOutcome::Unknown;
    std::string message;
    bool unsupported = false;
};

struct DownloadResult {
    bool success = false;
    std::string message;
    PokemonPayload payload;
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
    std::array<PokemonSummary, BoxSlotCount> pokemon{};
    std::array<PokemonPayload, BoxSlotCount> payloads{};
};

struct UpdateAsset {
    std::string sha256;
    std::string signature;
    std::uint32_t size = 0;
};

struct ClientUpdate {
    bool success = false;
    std::string message;
    std::string tag;
    std::string version;
    UpdateAsset cia;
    UpdateAsset threeDsx;
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
    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    AuthResult login(const std::string& username, const std::string& password);
    AuthResult registerAccount(const std::string& username, const std::string& email, const std::string& password,
                               const std::string& deviceFingerprint);
    AuthResult refresh(const std::string& refreshToken);
    AuthResult requestPasswordReset(const std::string& email);

    UploadResult uploadPokemon(const std::vector<UploadPokemon>& pokemon, const std::string& accessToken);
    DownloadResult downloadPokemon(CloudSlot slot, const std::string& accessToken);
    DeleteResult deleteCloudPokemon(CloudSlot slot, const std::string& accessToken);
    DeleteResult deleteCloudPokemonBatch(const std::vector<CloudSlot>& slots, const std::string& accessToken);
    MoveResult moveCloudPokemon(CloudSlot from, CloudSlot to, const std::string& accessToken);
    BoxListResult listCloudBox(std::uint16_t boxPosition, const std::string& accessToken);
    RenameBoxResult renameBox(std::uint16_t boxPosition, const std::string& name, const std::string& accessToken);
    BoxNamesResult listBoxNames(const std::string& accessToken);

    ClientUpdate latestClientUpdate();
    FileDownloadResult downloadClientUpdate(const std::string& tag, const std::string& assetName,
                                            const std::string& destination, std::uint32_t expectedSize);

    bool consumeSessionRejected();

private:
    enum class Method { Get, Post, Put, Delete };

    struct HttpResult {
        bool success = false;
        std::uint32_t status = 0;
        std::string body;
        std::string message;
        bool sent = false;
        std::uint32_t retryAfterSeconds = 0;

        RequestOutcome failureOutcome() const { return sent ? RequestOutcome::Unknown : RequestOutcome::Refused; }
    };

    static constexpr std::uint64_t DefaultTimeoutNanoseconds = 30'000'000'000ULL;
    static constexpr std::uint64_t UploadTimeoutNanoseconds = 150'000'000'000ULL;

    AuthResult postAuth(const std::string& path, const std::string& body);
    static DeleteResult deleteResult(const HttpResult& response, bool batch);
    HttpResult request(Method method, const std::string& path, const std::string& body = {},
                       const std::string& accessToken = {},
                       std::uint64_t timeoutNanoseconds = DefaultTimeoutNanoseconds);
    HttpResult requestOnce(Method method, const std::string& path, const std::string& body,
                           const std::string& accessToken, std::uint64_t timeoutNanoseconds);
    void syncClock();
    std::uint64_t signedTimestampSeconds();

    bool initialized_ = false;
    std::atomic<bool> sessionRejected_{false};
    LightLock clockLock_;
    std::optional<std::int64_t> clockDeltaMs_;
    std::uint64_t lastClockSyncAttemptMs_ = 0;
};
