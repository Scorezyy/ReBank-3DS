#pragma once

#include "core/AsyncTask.hpp"
#include "gui/Screen.hpp"
#include "gui/elements/TitleIcon.hpp"
#include "update/PluginGames.hpp"
#include "update/PluginInstaller.hpp"

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class PluginScreen : public Screen {
public:
    using Screen::Screen;

    void open();
    void update(const InputFrame& input) override;
    void renderTop(float eyeOffset) override;
    void render() override;

private:
    enum class Phase { Loading, Select, Installing, Done };

    struct Tile {
        std::optional<std::string> installed;
        bool selected = false;
    };

    struct JobResult {
        PluginKind kind = PluginKind::LiveBox;
        std::string version;
        PluginInstallOutcome outcome = PluginInstallOutcome::Unreachable;
    };

    struct InstallRun {
        std::vector<JobResult> results;
        PluginLoaderState loader = PluginLoaderState::Manual;
    };

    void close();
    void refreshInstalled();
    void requestIcons();
    void pollIcons();
    void pollCatalog();
    void startCatalog();
    std::vector<std::string> wrap(std::string_view value, float size, float width);
    void startInstall();
    void showPage(std::size_t page);
    void moveFocus(int dx, int dy);
    void toggle(std::size_t index);
    void updateSelect(const InputFrame& input);
    const CatalogPlugin* catalogFor(PluginKind kind) const;
    bool selectable(std::size_t index) const;
    std::size_t selectedCount() const;
    std::size_t pageCount() const;
    InstallRun install(const std::string& accessToken, std::vector<PluginInstallJob> jobs);

    void drawTile(std::size_t index, const UiRect& rect);
    void drawFooterButtons();
    void drawTopHeader(float eyeOffset);
    void drawSelectTop(float eyeOffset);
    void drawInstallTop(float eyeOffset);
    void drawDoneTop(float eyeOffset);

    Phase phase_ = Phase::Loading;
    PluginCatalog catalog_;
    std::string catalogAccount_;
    bool discardCatalog_ = false;
    std::vector<Tile> tiles_;
    std::vector<std::unique_ptr<TitleIcon>> icons_;
    std::size_t focus_ = 0;
    std::size_t page_ = 0;
    std::vector<PluginKind> jobKinds_;
    std::atomic<std::uint32_t> currentJob_{0};
    PluginInstallProgress progress_;
    InstallRun run_;
    AsyncTask<PluginCatalog> catalogTask_;
    AsyncTask<InstallRun> installTask_;
    AsyncTask<std::vector<std::optional<TitleIcon::Pixels>>> iconTask_;
    bool iconsRequested_ = false;
};
