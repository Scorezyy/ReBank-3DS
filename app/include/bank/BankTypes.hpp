#pragma once

#include "save/SaveAdapter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class HandSource {
    Local,
    Cloud,
    Party
};

enum class StoragePane {
    Local,
    Cloud,
    Party
};

struct SwapOrigin {
    bool active = false;
    HandSource source = HandSource::Local;
    std::size_t sourceIndex = 0;
    std::size_t sourceLocalBox = 0;
    std::uint16_t sourceCloudBox = 0;
    bool sourceTrash = false;
    PokemonSummary summary;
    PokemonPayload payload;
};

struct Hand {
    bool active = false;
    HandSource source = HandSource::Local;
    std::size_t sourceIndex = 0;
    std::size_t sourceLocalBox = 0;
    std::uint16_t sourceCloudBox = 0;
    bool sourceTrash = false;
    PokemonSummary summary;
    PokemonPayload payload;
    bool payloadKnown = false;
    SwapOrigin swapOrigin;
};

struct LocalBoxDraft {
    std::array<PokemonSummary, 30> summaries{};
    std::array<PokemonPayload, 30> payloads{};
};

struct PartyDraft {
    std::array<PokemonSummary, 6> summaries{};
    std::array<PokemonPayload, 6> payloads{};
};

struct CloudBoxDraft {
    std::array<PokemonSummary, 30> summaries{};
    std::array<PokemonPayload, 30> pending{};
    std::array<PokemonSummary, 30> baseline{};
    std::array<PokemonPayload, 30> payloads{};
};

struct CommitSkippedItem {
    std::string nickname;
    std::string location;
    std::string reason;
};

struct CommitResult {
    bool success = false;
    std::string message;
    std::string problemPokemon;
    std::string problemLocation;
    std::string problemReason;
    std::size_t uploads = 0;
    std::size_t downloads = 0;
    std::size_t deletes = 0;
    std::vector<CommitSkippedItem> skipped;
};
