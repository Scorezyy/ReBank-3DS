#pragma once

#include "save/SaveAdapter.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

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

struct Hand {
    bool active = false;
    HandSource source = HandSource::Local;
    std::size_t sourceIndex = 0;
    std::uint16_t sourceCloudBox = 0;
    PokemonSummary summary;
    PokemonPayload payload;
    bool payloadKnown = false;
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

struct CommitResult {
    bool success = false;
    std::string message;
    std::string problemPokemon;
    std::string problemLocation;
    std::string problemReason;
    std::size_t uploads = 0;
    std::size_t downloads = 0;
    std::size_t deletes = 0;
};
