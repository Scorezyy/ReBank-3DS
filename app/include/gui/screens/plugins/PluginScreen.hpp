#pragma once

#include "gui/Screen.hpp"
#include "gui/icons/IconTexture.hpp"
#include "gui/icons/TitleIcon.hpp"
#include "gui/screens/plugins/PluginDetailsView.hpp"
#include "platform/AsyncTask.hpp"
#include "plugin/PluginSetup.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class PluginScreen : public Screen {
public:
    explicit PluginScreen(AppContext& app);

    void enter() override;
    void update(const InputFrame& input) override;
    void renderTop(float eyeOffset) override;
    void render() override;

private:
    enum class Phase { Loading, Select, Installing, Done };

    struct Tile {
        std::optional<std::string> installed;
        bool selected = false;
    };

    using IconList = std::vector<std::optional<IconPixels>>;

    void close();
    void refreshInstalled();
    void requestIcons();
    void pollIcons();
    void pollCatalog();
    void pollInstall();
    void startInstall();
    void updateSelect(const InputFrame& input);
    void moveFocus(int dx, int dy);
    void showPage(std::size_t page);
    void toggle(std::size_t index);
    bool selectable(std::size_t index) const;
    std::vector<std::size_t> selectedGames() const;
    std::size_t pageCount() const;
    const IconTexture* iconAt(std::size_t index) const;

    void drawPage();
    void drawTile(std::size_t index, const UiRect& rect);
    void drawFooterButtons();

    PluginSetup setup_;
    PluginDetailsView details_;
    Phase phase_ = Phase::Loading;
    std::vector<Tile> tiles_;
    std::vector<std::unique_ptr<IconTexture>> icons_;
    std::size_t focus_ = 0;
    std::size_t page_ = 0;
    AsyncTask<IconList> iconTask_;
    bool iconsRequested_ = false;
};
