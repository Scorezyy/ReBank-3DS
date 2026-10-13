#pragma once

#include "core/AsyncTask.hpp"
#include "gui/Screen.hpp"
#include "update/PluginInstaller.hpp"

class PluginScreen : public Screen {
public:
    using Screen::Screen;

    void open();
    void update(const InputFrame& input) override;
    void renderTop(float eyeOffset) override;
    void render() override;

private:
    enum class Phase { Ask, Installing, Done, Failed };

    void startInstall();
    void close();
    void drawDescription(float eyeOffset);
    void drawProgress(float eyeOffset);
    void drawResult(float eyeOffset);
    TextId failureReason() const;

    Phase phase_ = Phase::Ask;
    PluginInstallProgress progress_;
    PluginInstallResult result_;
    AsyncTask<PluginInstallResult> task_;
};
