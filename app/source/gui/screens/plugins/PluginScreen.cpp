#include "gui/screens/plugins/PluginScreen.hpp"

#include "gui/screens/plugins/PluginPalette.hpp"
#include "gui/widgets/Shapes.hpp"

#include <algorithm>

using namespace Gui;
using namespace PluginPalette;

namespace {
constexpr std::size_t Columns = 2;
constexpr std::size_t Rows = 6;
constexpr std::size_t TilesPerPage = Columns * Rows;
constexpr std::size_t MinimumPages = 2;
constexpr float TileLeft = 8.0F;
constexpr float TileTop = 6.0F;
constexpr float TileWidth = 148.0F;
constexpr float TileHeight = 28.0F;
constexpr float TilePitchX = 156.0F;
constexpr float TilePitchY = 31.0F;
constexpr float FooterTop = 197.0F;
constexpr UiRect PluginBackButton{8.0F, 204.0F, 100.0F, 30.0F};
constexpr UiRect PluginInstallButton{116.0F, 204.0F, 196.0F, 30.0F};
constexpr UiRect DoneButton{24.0F, 150.0F, 272.0F, 44.0F};

UiRect tileRect(std::size_t slot) {
    return {TileLeft + static_cast<float>(slot % Columns) * TilePitchX,
            TileTop + static_cast<float>(slot / Columns) * TilePitchY, TileWidth, TileHeight};
}

void drawPlaceholder(const UiRect& rect, UiRenderer& ui) {
    drawCard(rect, CornerRadius, 1.0F, LockedFill, PageFill);
    ui.drawCentered("- - - - - - - -", rect.x + rect.width * 0.5F, rect.y + 6.0F, 0.48F, LockedInk);
}

void drawCheck(float x, float y, u32 color) {
    C2D_DrawLine(x, y + 4.0F, color, x + 4.0F, y + 8.0F, color, 2.2F, 0.3F);
    C2D_DrawLine(x + 4.0F, y + 8.0F, color, x + 11.0F, y, color, 2.2F, 0.3F);
}

void drawLock(float x, float y, u32 color) {
    C2D_DrawRectSolid(x, y + 5.0F, 0.3F, 10.0F, 7.0F, color);
    C2D_DrawRectSolid(x + 1.5F, y, 0.3F, 1.5F, 6.0F, color);
    C2D_DrawRectSolid(x + 7.0F, y, 0.3F, 1.5F, 6.0F, color);
    C2D_DrawRectSolid(x + 1.5F, y - 1.0F, 0.3F, 7.0F, 1.5F, color);
}
}

PluginScreen::PluginScreen(AppContext& app)
    : Screen(app), setup_(app_.releaseApi), details_(app_.ui, app_.text, setup_) {}

void PluginScreen::enter() {
    setup_.loadCatalog(app_.account.session.accountId, app_.account.session.accessToken);
    phase_ = setup_.catalogLoaded() || !setup_.catalogRunning() ? Phase::Select : Phase::Loading;
    focus_ = 0;
    page_ = 0;
    setup_.clearRun();
    tiles_.assign(PluginGames::all().size(), {});
    refreshInstalled();
    requestIcons();
    app_.status.clear();
}

void PluginScreen::close() {
    app_.navigator.show(ScreenId::GameSelect);
}

void PluginScreen::pollCatalog() {
    if (!setup_.pollCatalog()) {
        return;
    }
    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        tiles_[index].selected = tiles_[index].selected && selectable(index);
    }
    if (phase_ == Phase::Loading) {
        phase_ = Phase::Select;
    }
}

void PluginScreen::requestIcons() {
    if (iconsRequested_) {
        return;
    }
    iconsRequested_ = iconTask_.start([]() {
        const InstalledTitles installed = InstalledTitles::scan();
        IconList pixels;
        for (const PluginGame& game : PluginGames::all()) {
            pixels.push_back(TitleIcon::read(game.titleIds, installed));
        }
        return pixels;
    });
}

void PluginScreen::pollIcons() {
    IconList pixels;
    if (!iconTask_.poll(pixels)) {
        return;
    }
    icons_.clear();
    for (const std::optional<IconPixels>& icon : pixels) {
        auto texture = std::make_unique<IconTexture>();
        if (icon) {
            texture->create(*icon, IconTexture::Layout::Tiled);
        }
        icons_.push_back(std::move(texture));
    }
}

const IconTexture* PluginScreen::iconAt(std::size_t index) const {
    return index < icons_.size() ? icons_[index].get() : nullptr;
}

void PluginScreen::refreshInstalled() {
    const auto games = PluginGames::all();
    for (std::size_t index = 0; index < games.size(); ++index) {
        tiles_[index].installed = PluginGames::installedVersion(games[index]);
    }
}

bool PluginScreen::selectable(std::size_t index) const {
    return setup_.installable(PluginGames::all()[index].kind);
}

std::vector<std::size_t> PluginScreen::selectedGames() const {
    std::vector<std::size_t> selected;
    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        if (tiles_[index].selected && selectable(index)) {
            selected.push_back(index);
        }
    }
    return selected;
}

std::size_t PluginScreen::pageCount() const {
    return std::max(MinimumPages, (tiles_.size() + TilesPerPage - 1) / TilesPerPage);
}

void PluginScreen::toggle(std::size_t index) {
    focus_ = index;
    if (selectable(index)) {
        tiles_[index].selected = !tiles_[index].selected;
    }
}

void PluginScreen::showPage(std::size_t page) {
    page_ = page;
    if (page * TilesPerPage < tiles_.size()) {
        focus_ = page * TilesPerPage;
    }
}

void PluginScreen::moveFocus(int dx, int dy) {
    const std::size_t slot = focus_ % TilesPerPage;
    const int column = static_cast<int>(slot % Columns) + dx;
    const int row = static_cast<int>(slot / Columns) + dy;
    std::size_t page = page_;
    if (column < 0 && page > 0) {
        --page;
    } else if (column >= static_cast<int>(Columns) && page + 1 < pageCount()) {
        ++page;
    }
    const auto clampedColumn = static_cast<std::size_t>(std::clamp(column, 0, static_cast<int>(Columns) - 1));
    const auto clampedRow = static_cast<std::size_t>(std::clamp(row, 0, static_cast<int>(Rows) - 1));
    const std::size_t target = page * TilesPerPage + clampedRow * Columns + clampedColumn;
    if (target < tiles_.size()) {
        focus_ = target;
        page_ = page;
    }
}

void PluginScreen::update(const InputFrame& input) {
    pollIcons();
    if (phase_ != Phase::Installing) {
        pollCatalog();
    }
    switch (phase_) {
        case Phase::Loading:
            if (input.pressed(KEY_B)) {
                close();
            }
            return;
        case Phase::Select:
            updateSelect(input);
            return;
        case Phase::Installing:
            pollInstall();
            return;
        case Phase::Done:
            if (input.pressed(KEY_A | KEY_B) || input.tapped(DoneButton)) {
                phase_ = Phase::Select;
            }
            return;
    }
}

void PluginScreen::updateSelect(const InputFrame& input) {
    if (input.pressed(KEY_B) || input.tapped(PluginBackButton)) {
        close();
        return;
    }
    if ((input.pressed(KEY_X) || input.tapped(PluginInstallButton)) && !selectedGames().empty()) {
        startInstall();
        return;
    }
    if (input.pressed(KEY_L) && page_ > 0) {
        showPage(page_ - 1);
    } else if (input.pressed(KEY_R) && page_ + 1 < pageCount()) {
        showPage(page_ + 1);
    }
    if (input.pressed(KEY_DLEFT)) {
        moveFocus(-1, 0);
    } else if (input.pressed(KEY_DRIGHT)) {
        moveFocus(1, 0);
    } else if (input.pressed(KEY_DUP)) {
        moveFocus(0, -1);
    } else if (input.pressed(KEY_DDOWN)) {
        moveFocus(0, 1);
    }
    if (input.pressed(KEY_A)) {
        toggle(focus_);
    }
    for (std::size_t slot = 0; slot < TilesPerPage && input.touched(); ++slot) {
        const std::size_t index = page_ * TilesPerPage + slot;
        if (index < tiles_.size() && input.tapped(tileRect(slot))) {
            toggle(index);
        }
    }
}

void PluginScreen::startInstall() {
    phase_ = setup_.install(selectedGames(), app_.account.session.accessToken) ? Phase::Installing : Phase::Done;
}

void PluginScreen::pollInstall() {
    if (!setup_.pollInstall()) {
        return;
    }
    refreshInstalled();
    for (Tile& tile : tiles_) {
        tile.selected = false;
    }
    phase_ = Phase::Done;
}

void PluginScreen::renderTop(float eyeOffset) {
    const std::optional<std::string> pageLabel = phase_ == Phase::Select
        ? std::optional(format(TextId::PluginsPage, {std::to_string(page_ + 1), std::to_string(pageCount())}))
        : std::nullopt;
    details_.drawHeader(eyeOffset, pageLabel);
    switch (phase_) {
        case Phase::Loading:
            details_.drawLoading(eyeOffset);
            return;
        case Phase::Select:
            details_.drawGame(eyeOffset, PluginGames::all()[focus_], tiles_[focus_].installed, iconAt(focus_),
                              selectedGames().size());
            return;
        case Phase::Installing:
            details_.drawInstalling(eyeOffset);
            return;
        case Phase::Done:
            details_.drawResult(eyeOffset);
            return;
    }
}

void PluginScreen::render() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.02F, 320.0F, 240.0F, PageFill);
    switch (phase_) {
        case Phase::Loading:
            ui().drawCentered(text(TextId::PluginPleaseWait), 160.0F, 104.0F, 0.6F, CardInk);
            return;
        case Phase::Select:
            drawPage();
            drawFooterButtons();
            return;
        case Phase::Installing:
            ui().drawCentered(text(TextId::PluginPleaseWait), 160.0F, 92.0F, 0.62F, CardInk);
            ui().drawCentered(text(TextId::PluginKeepPowerOn), 160.0F, 122.0F, 0.5F, CardMuted);
            return;
        case Phase::Done:
            ui().drawCentered(text(TextId::PluginsDoneTitle), 160.0F, 84.0F, 0.62F, CardInk);
            drawCard(DoneButton, CornerRadius, 0.0F, WarmBrand, WarmBrand);
            ui().drawCentered("A  " + std::string(text(TextId::Ok)), 160.0F, DoneButton.y + 13.0F, 0.52F, Surface);
            return;
    }
}

void PluginScreen::drawPage() {
    for (std::size_t slot = 0; slot < TilesPerPage; ++slot) {
        const std::size_t index = page_ * TilesPerPage + slot;
        if (index < tiles_.size()) {
            drawTile(index, tileRect(slot));
        } else {
            drawPlaceholder(tileRect(slot), ui());
        }
    }
}

void PluginScreen::drawTile(std::size_t index, const UiRect& rect) {
    const PluginGame& game = PluginGames::all()[index];
    const Tile& tile = tiles_[index];
    const CatalogPlugin* plugin = setup_.find(game.kind);
    const bool locked = plugin != nullptr && !plugin->access;
    const bool focused = index == focus_;
    u32 fill = CardFill;
    u32 border = focused ? WarmBrandDark : CardBorder;
    u32 ink = CardInk;
    u32 muted = CardMuted;
    if (locked) {
        fill = LockedFill;
        border = focused ? LockedInk : LockedFill;
        ink = LockedInk;
        muted = LockedInk;
    } else if (tile.selected) {
        fill = WarmBrand;
        border = focused ? WarmBrandDark : WarmBrand;
        ink = Surface;
        muted = SelectedMuted;
    }
    drawCard(rect, CornerRadius, focused ? 2.0F : 1.0F, border, fill);
    ui().drawText(text(game.name), rect.x + 9.0F, rect.y + 1.0F, 0.48F, ink);
    std::string version;
    if (plugin != nullptr && plugin->available) {
        version = "v" + plugin->version;
    } else if (tile.installed && !tile.installed->empty()) {
        version = "v" + *tile.installed;
    }
    ui().drawText(version, rect.x + 9.0F, rect.y + 15.0F, 0.42F, muted);
    if (locked) {
        drawLock(rect.x + rect.width - 18.0F, rect.y + 9.0F, LockedInk);
    } else if (tile.installed) {
        drawCheck(rect.x + rect.width - 20.0F, rect.y + 10.0F, tile.selected ? Surface : InstalledGreen);
    }
}

void PluginScreen::drawFooterButtons() {
    C2D_DrawRectSolid(0.0F, FooterTop, 0.05F, 320.0F, 1.0F, CardBorder);
    drawCard(PluginBackButton, CornerRadius, 1.5F, CardBorder, CardFill);
    ui().drawCentered("B  " + std::string(text(TextId::PluginsBack)), PluginBackButton.x + PluginBackButton.width * 0.5F,
                      PluginBackButton.y + 6.0F, 0.52F, CardInk);
    const std::size_t selected = selectedGames().size();
    const bool ready = selected > 0;
    drawCard(PluginInstallButton, CornerRadius, 0.0F, ready ? WarmBrand : DisabledFill, ready ? WarmBrand : DisabledFill);
    std::string label = "X  " + std::string(text(TextId::PluginsInstall));
    if (ready) {
        label += "  (" + std::to_string(selected) + ")";
    }
    ui().drawCentered(label, PluginInstallButton.x + PluginInstallButton.width * 0.5F, PluginInstallButton.y + 6.0F, 0.52F,
                      ready ? Surface : LockedInk);
}
