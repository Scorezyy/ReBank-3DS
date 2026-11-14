#pragma once

#include "network/ApiTypes.hpp"
#include "network/HttpClient.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CloudApi {
public:
    explicit CloudApi(HttpClient& http) : http_(http) {}

    UploadResult upload(const std::vector<UploadPokemon>& pokemon, const std::string& accessToken);
    DownloadResult download(CloudSlot slot, const std::string& accessToken);
    DeleteResult remove(CloudSlot slot, const std::string& accessToken);
    DeleteResult remove(const std::vector<CloudSlot>& slots, const std::string& accessToken);
    MoveResult move(CloudSlot from, CloudSlot to, const std::string& accessToken);
    ClaimResult claim(const std::vector<ClaimRequest>& requests, const std::string& accessToken,
                      const std::optional<TrainerInfo>& trainer = std::nullopt);
    BoxListResult listBox(std::uint16_t boxPosition, const std::string& accessToken);
    BoxNamesResult listBoxNames(const std::string& accessToken);
    RenameBoxResult renameBox(std::uint16_t boxPosition, const std::string& name, const std::string& accessToken);

private:
    HttpClient& http_;
};
