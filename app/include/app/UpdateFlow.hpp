#pragma once

#include "app/AppContext.hpp"
#include "network/api/ReleaseApi.hpp"
#include "platform/AsyncTask.hpp"
#include "update/UpdateInstaller.hpp"

#include <cstdint>
#include <optional>
#include <string>

class UpdateFlow {
public:
    enum class Notice { None, Showing, Ended };

    UpdateFlow(ReleaseApi& api, std::string executablePath, UpdateState& state)
        : api_(api), executablePath_(std::move(executablePath)), state_(state) {}

    bool start();
    std::optional<UpdateInstallResult> poll();
    Notice pollNotice();
    bool running() const { return task_.running() || noticeUntil_ != 0; }
    bool installed() const { return state_.progress.stage.load() == UpdateStage::Installed; }

private:
    ReleaseApi& api_;
    std::string executablePath_;
    UpdateState& state_;
    AsyncTask<UpdateInstallResult> task_;
    std::uint64_t noticeUntil_ = 0;
};
