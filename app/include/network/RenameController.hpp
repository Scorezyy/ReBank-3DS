#pragma once

#include "core/AsyncTask.hpp"
#include "network/ApiClient.hpp"

#include <cstdint>
#include <string>

// Renames a cloud box on a background thread.
class RenameController {
public:
    bool begin(ApiClient& api, std::uint16_t position, std::string name, std::string accessToken);
    // Call once per frame. Returns true the moment a finished rename result
    // becomes available, with `position` set to the box that was renamed.
    bool poll(std::uint16_t& position, RenameBoxResult& result);
    bool isRunning() const { return task_.running(); }

private:
    struct Job {
        std::uint16_t position = 0;
        RenameBoxResult result;
    };

    AsyncTask<Job> task_;
};
