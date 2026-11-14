#include "app/UpdateFlow.hpp"

#include "platform/Clock.hpp"
#include "platform/Logger.hpp"

namespace {
constexpr double InstalledNoticeSeconds = 3.5;
constexpr double FailedNoticeSeconds = 10.0;
}

bool UpdateFlow::start() {
    const bool started = task_.start([this]() {
        return UpdateInstaller::run(api_, executablePath_, state_.homebrew, state_.progress);
    });
    if (!started) {
        Logger::instance().warning("Update worker creation failed");
    }
    return started;
}

std::optional<UpdateInstallResult> UpdateFlow::poll() {
    UpdateInstallResult result;
    if (!task_.poll(result)) {
        return std::nullopt;
    }
    if (result.updated) {
        Logger::instance().info(result.message);
        noticeUntil_ = Clock::after(InstalledNoticeSeconds);
    } else if (!result.success) {
        Logger::instance().warning(result.message);
    }
    if (state_.progress.stage.load() == UpdateStage::Failed) {
        state_.keptCia = result.keptForManualInstall;
        noticeUntil_ = Clock::after(FailedNoticeSeconds);
    }
    return result;
}

UpdateFlow::Notice UpdateFlow::pollNotice() {
    if (noticeUntil_ == 0) {
        return Notice::None;
    }
    if (!Clock::reached(noticeUntil_)) {
        return Notice::Showing;
    }
    noticeUntil_ = 0;
    return Notice::Ended;
}
