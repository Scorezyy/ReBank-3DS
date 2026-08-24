#pragma once

#include "core/AsyncTask.hpp"
#include "network/ApiClient.hpp"
#include "network/UpdateInstaller.hpp"

#include <string>

// Runs the self-update check/install on a background thread.
class UpdateController {
public:
    // Returns false if a check/install is already running or the thread
    // could not be created.
    bool begin(ApiClient& api, std::string executablePath, bool homebrew);
    // Call once per frame. Returns true the moment a finished check/install
    // result becomes available.
    bool poll(UpdateInstallResult& result);
    bool isRunning() const { return task_.running(); }

private:
    AsyncTask<UpdateInstallResult> task_;
};
