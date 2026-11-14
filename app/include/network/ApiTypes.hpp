#pragma once

#include "save/pokemon/PokemonData.hpp"

#include <array>
#include <cstdint>
#include <string>
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

enum class ClaimState : std::uint8_t {
    Claimed,
    Changed,
    Locked,
    Empty
};

struct ClaimRequest {
    CloudSlot slot;
    std::vector<std::uint8_t> payload;
};

struct ClaimResult {
    RequestOutcome outcome = RequestOutcome::Unknown;
    std::string message;
    std::vector<ClaimState> states;
    std::vector<std::vector<std::uint8_t>> received;
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

struct CatalogPlugin {
    std::string id;
    bool access = false;
    bool available = false;
    std::string tag;
    std::string version;
    UpdateAsset file;
};

struct PluginCatalog {
    bool success = false;
    std::string message;
    std::vector<CatalogPlugin> plugins;
};

struct FileDownloadResult {
    bool success = false;
    std::string message;
    std::uint32_t size = 0;
};

struct PolicyBlock {
    enum class Kind { Title, Heading, Subheading, Paragraph, Bullet, Row };
    Kind kind = Kind::Paragraph;
    std::string text;
    std::vector<std::string> cells;
};
