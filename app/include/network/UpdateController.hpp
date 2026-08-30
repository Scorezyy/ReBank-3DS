#pragma once

#include "core/AsyncTask.hpp"
#include "network/ApiClient.hpp"
#include "network/UpdateInstaller.hpp"

#include <string>

class UpdateController {
public:

    bool begin(ApiClient& api, std::string executablePath, bool homebrew);

    bool poll(UpdateInstallResult& result);
    bool isRunning() const { return task_.running(); }

private:
    AsyncTask<UpdateInstallResult> task_;
};
