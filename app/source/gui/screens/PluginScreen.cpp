#include "gui/screens/PluginScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "gui/elements/Shapes.hpp"
#include "gui/elements/TitleIcon.hpp"

#include <algorithm>
#include <array>
#include <map>

using namespace Gui;

namespace {
constexpr std::size_t Columns = 2;
constexpr std::size_t Rows = 6;
constexpr std::size_t TilesPerPage = Columns * Rows;
constexpr float TileLeft = 8.0F;
constexpr float TileTop = 6.0F;
constexpr float TileWidth = 148.0F;
constexpr float TileHeight = 28.0F;
constexpr float TilePitchX = 156.0F;
constexpr float TilePitchY = 31.0F;
constexpr float CornerRadius = 6.0F;
constexpr float FooterTop = 197.0F;
constexpr UiRect PluginBackButton{8.0F, 204.0F, 100.0F, 30.0F};
constexpr UiRect PluginInstallButton{116.0F, 204.0F, 196.0F, 30.0F};
constexpr UiRect DoneButton{24.0F, 150.0F, 272.0F, 44.0F};
constexpr u32 PageFill = C2D_Color32(250, 246, 242, 255);
constexpr u32 CardFill = C2D_Color32(255, 255, 255, 255);
constexpr u32 CardBorder = C2D_Color32(232, 224, 216, 255);
constexpr u32 CardInk = C2D_Color32(44, 32, 26, 255);
constexpr u32 CardMuted = C2D_Color32(150, 138, 128, 255);
constexpr u32 SelectedMuted = C2D_Color32(255, 222, 200, 255);
constexpr u32 LockedFill = C2D_Color32(241, 237, 233, 255);
constexpr u32 LockedInk = C2D_Color32(178, 170, 162, 255);
constexpr u32 InstalledGreen = C2D_Color32(38, 160, 86, 255);
constexpr u32 DisabledFill = C2D_Color32(236, 230, 224, 255);
constexpr u32 StatusGreen = C2D_Color32(38, 150, 82, 255);
constexpr u32 MinimumPages = 2;
constexpr u32 TrackColor = C2D_Color32(232, 224, 216, 255);
constexpr float BarX = 60.0F;
constexpr float BarY = 146.0F;
constexpr float BarWidth = 280.0F;
constexpr float BarHeight = 12.0F;
constexpr std::uint32_t BytesPerKb = 1024;
constexpr std::array<TextId, 3> NextSteps = {TextId::PluginNextEnable, TextId::PluginNextKeys, TextId::PluginNextStart};
constexpr std::array<TextId, 2> ReadySteps = {TextId::PluginLoaderEnabled, TextId::PluginNextJustStart};
constexpr std::array<TextId, 3> UpdateLumaSteps = {TextId::PluginLumaTooOld, TextId::PluginLumaUpdate, TextId::PluginLumaThenStart};

UiRect tileRect(std::size_t slot) {
    return {TileLeft + static_cast<float>(slot % Columns) * TilePitchX, TileTop + static_cast<float>(slot / Columns) * TilePitchY,
            TileWidth, TileHeight};
}

void drawCard(const UiRect& rect, float border, u32 borderColor, u32 fill) {
    Gui::drawRoundedRect(rect.x, rect.y, rect.width, rect.height, CornerRadius, 0.1F, borderColor);
    Gui::drawRoundedRect(rect.x + border, rect.y + border, rect.width - border * 2.0F, rect.height - border * 2.0F,
                         CornerRadius - border, 0.11F, fill);
}

void drawPlaceholder(const UiRect& rect, UiRenderer& renderer) {
    drawCard(rect, 1.0F, LockedFill, PageFill);
    renderer.drawCentered("- - - - - - - -", rect.x + rect.width * 0.5F, rect.y + 6.0F, 0.48F, LockedInk);
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

std::string_view pluginName(PluginKind kind) {
    return kind == PluginKind::Acnl ? "ReBank ACNL" : "ReBank LiveBox";
}
}

void PluginScreen::open() {
    if (catalogAccount_ != app_.session_.accountId) {
        catalog_ = {};
        catalogAccount_ = app_.session_.accountId;
        discardCatalog_ = catalogTask_.running();
    }
    phase_ = catalog_.success ? Phase::Select : Phase::Loading;
    focus_ = 0;
    page_ = 0;
    run_ = {};
    tiles_.assign(PluginGames::all().size(), {});
    refreshInstalled();
    requestIcons();
    app_.status_.clear();
    app_.screen_ = ScreenId::Plugin;
    if (!catalogTask_.running()) {
        startCatalog();
    }
}

void PluginScreen::startCatalog() {
    const std::string token = app_.session_.accessToken;
    if (!catalogTask_.start([this, token]() { return app_.api_.pluginCatalog(token); })) {
        Logger::instance().warning("Plugin catalog worker creation failed");
        phase_ = Phase::Select;
    }
}

void PluginScreen::pollCatalog() {
    PluginCatalog fresh;
    if (!catalogTask_.poll(fresh)) {
        return;
    }
    if (discardCatalog_) {
        discardCatalog_ = false;
        startCatalog();
        return;
    }
    if (!fresh.success) {
        Logger::instance().warning("Plugin catalog: " + fresh.message);
    }
    if (fresh.success || !catalog_.success) {
        catalog_ = std::move(fresh);
    }
    for (std::size_t index = 0; index < tiles_.size(); ++index) {
        tiles_[index].selected = tiles_[index].selected && selectable(index);
    }
    if (phase_ == Phase::Loading) {
        phase_ = Phase::Select;
    }
}

void PluginScreen::close() {
    app_.screen_ = ScreenId::GameSelect;
}

void PluginScreen::requestIcons() {
    if (iconsRequested_) {
        return;
    }
    iconsRequested_ = true;
    const bool started = iconTask_.start([]() {
        const InstalledTitles installed = InstalledTitles::scan();
        std::vector<std::optional<TitleIcon::Pixels>> pixels;
        for (const PluginGame& game : PluginGames::all()) {
            pixels.push_back(TitleIcon::read(game.folders, installed));
        }
        return pixels;
    });
    if (!started) {
        iconsRequested_ = false;
    }
}

void PluginScreen::pollIcons() {
    std::vector<std::optional<TitleIcon::Pixels>> pixels;
    if (!iconTask_.poll(pixels)) {
        return;
    }
    icons_.clear();
    for (const std::optional<TitleIcon::Pixels>& icon : pixels) {
        auto texture = std::make_unique<TitleIcon>();
        if (icon) {
            texture->create(*icon);
        }
        icons_.push_back(std::move(texture));
    }
}

std::vector<std::string> PluginScreen::wrap(std::string_view value, float size, float width) {
    if (ui().textWidth(value, size) <= width) {
        return {std::string(value)};
    }
    std::size_t split = std::string_view::npos;
    for (std::size_t space = value.find(' '); space != std::string_view::npos; space = value.find(' ', space + 1)) {
        if (ui().textWidth(value.substr(0, space), size) > width) {
            break;
        }
        split = space;
    }
    if (split == std::string_view::npos) {
        return {std::string(value)};
    }
    return {std::string(value.substr(0, split)), std::string(value.substr(split + 1))};
}

void PluginScreen::refreshInstalled() {
    const auto games = PluginGames::all();
    for (std::size_t index = 0; index < games.size(); ++index) {
        tiles_[index].installed = PluginGames::installedVersion(games[index]);
    }
}

const CatalogPlugin* PluginScreen::catalogFor(PluginKind kind) const {
    const std::string_view id = PluginGames::catalogId(kind);
    for (const CatalogPlugin& plugin : catalog_.plugins) {
        if (plugin.id == id) {
            return &plugin;
        }
    }
    return nullptr;
}

bool PluginScreen::selectable(std::size_t index) const {
    const CatalogPlugin* plugin = catalogFor(PluginGames::all()[index].kind);
    return plugin != nullptr && plugin->access && plugin->available;
}

std::size_t PluginScreen::selectedCount() const {
    return static_cast<std::size_t>(std::count_if(tiles_.begin(), tiles_.end(), [](const Tile& tile) { return tile.selected; }));
}

std::size_t PluginScreen::pageCount() const {
    return std::max<std::size_t>(MinimumPages, (tiles_.size() + TilesPerPage - 1) / TilesPerPage);
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
    const int clampedColumn = std::clamp(column, 0, static_cast<int>(Columns) - 1);
    const int clampedRow = std::clamp(row, 0, static_cast<int>(Rows) - 1);
    const std::size_t target = page * TilesPerPage + static_cast<std::size_t>(clampedRow) * Columns + static_cast<std::size_t>(clampedColumn);
    if (target < tiles_.size()) {
        focus_ = target;
        page_ = page;
    }
}

void PluginScreen::updateSelect(const InputFrame& input) {
    if (input.pressed(KEY_B) || input.tapped(PluginBackButton)) {
        close();
        return;
    }
    if ((input.pressed(KEY_X) || input.tapped(PluginInstallButton)) && selectedCount() > 0) {
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
            if (installTask_.poll(run_)) {
                refreshInstalled();
                for (Tile& tile : tiles_) {
                    tile.selected = false;
                }
                phase_ = Phase::Done;
            }
            return;
        case Phase::Done:
            if (input.pressed(KEY_A | KEY_B) || input.tapped(DoneButton)) {
                phase_ = Phase::Select;
            }
            return;
    }
}

void PluginScreen::startInstall() {
    std::map<PluginKind, PluginInstallJob> grouped;
    const auto games = PluginGames::all();
    for (std::size_t index = 0; index < games.size(); ++index) {
        if (!tiles_[index].selected || !selectable(index)) {
            continue;
        }
        const CatalogPlugin& plugin = *catalogFor(games[index].kind);
        PluginInstallJob& job = grouped[games[index].kind];
        job.id = plugin.id;
        job.tag = plugin.tag;
        job.file = plugin.file;
        job.fileName = std::string(PluginGames::fileName(games[index].kind));
        job.folders.insert(job.folders.end(), games[index].folders.begin(), games[index].folders.end());
    }
    std::vector<PluginInstallJob> jobs;
    jobKinds_.clear();
    for (auto& [kind, job] : grouped) {
        jobKinds_.push_back(kind);
        jobs.push_back(std::move(job));
    }
    currentJob_.store(0);
    progress_.received.store(0);
    progress_.total.store(0);
    phase_ = Phase::Installing;
    const std::string token = app_.session_.accessToken;
    if (!installTask_.start([this, token, jobs = std::move(jobs)]() mutable { return install(token, std::move(jobs)); })) {
        Logger::instance().warning("Plugin install worker creation failed");
        run_ = {};
        phase_ = Phase::Done;
    }
}

PluginScreen::InstallRun PluginScreen::install(const std::string& accessToken, std::vector<PluginInstallJob> jobs) {
    InstallRun run;
    bool anyInstalled = false;
    for (std::size_t index = 0; index < jobs.size(); ++index) {
        currentJob_.store(static_cast<std::uint32_t>(index));
        const PluginInstallOutcome outcome = PluginInstaller::install(app_.api_, accessToken, jobs[index], progress_);
        anyInstalled = anyInstalled || outcome == PluginInstallOutcome::Installed;
        const std::string version = jobs[index].tag.size() > 1 ? jobs[index].tag.substr(1) : jobs[index].tag;
        run.results.push_back({jobKinds_[index], version, outcome});
    }
    run.loader = anyInstalled ? PluginLoader::enable() : PluginLoaderState::Manual;
    return run;
}

void PluginScreen::drawTile(std::size_t index, const UiRect& rect) {
    const PluginGame& game = PluginGames::all()[index];
    const Tile& tile = tiles_[index];
    const CatalogPlugin* plugin = catalogFor(game.kind);
    const bool locked = plugin != nullptr && !plugin->access;
    const bool focused = index == focus_ && phase_ == Phase::Select;
    u32 fill = CardFill;
    u32 border = focused ? WarmBrandDark : CardBorder;
    float thickness = focused ? 2.0F : 1.0F;
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
    drawCard(rect, thickness, border, fill);
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
    drawCard(PluginBackButton, 1.5F, CardBorder, CardFill);
    ui().drawCentered("B  " + std::string(text(TextId::PluginsBack)), PluginBackButton.x + PluginBackButton.width * 0.5F,
                      PluginBackButton.y + 6.0F, 0.52F, CardInk);
    const std::size_t selected = selectedCount();
    const bool ready = selected > 0;
    drawCard(PluginInstallButton, 0.0F, ready ? WarmBrand : DisabledFill, ready ? WarmBrand : DisabledFill);
    std::string label = "X  " + std::string(text(TextId::PluginsInstall));
    if (ready) {
        label += "  (" + std::to_string(selected) + ")";
    }
    ui().drawCentered(label, PluginInstallButton.x + PluginInstallButton.width * 0.5F, PluginInstallButton.y + 6.0F, 0.52F,
                      ready ? Surface : LockedInk);
}

void PluginScreen::drawTopHeader(float eyeOffset) {
    ui().drawText(text(TextId::PluginsTitle), 18.0F + eyeOffset, 12.0F, 0.66F, CardInk);
    if (phase_ == Phase::Select) {
        ui().drawRight(format(TextId::PluginsPage, {std::to_string(page_ + 1), std::to_string(pageCount())}), 382.0F + eyeOffset, 15.0F, 0.5F, CardMuted);
    }
    C2D_DrawRectSolid(18.0F + eyeOffset, 40.0F, 0.05F, 364.0F, 1.0F, CardBorder);
}

void PluginScreen::drawSelectTop(float eyeOffset) {
    const PluginGame& game = PluginGames::all()[focus_];
    const Tile& tile = tiles_[focus_];
    const CatalogPlugin* plugin = catalogFor(game.kind);
    const UiRect frame{18.0F + eyeOffset, 52.0F, 84.0F, 84.0F};
    drawCard(frame, 1.0F, CardBorder, focus_ < icons_.size() && icons_[focus_]->loaded() ? CardFill : LockedFill);
    if (focus_ < icons_.size() && icons_[focus_]->loaded()) {
        icons_[focus_]->draw(frame.x + 6.0F, frame.y + 6.0F, 0.2F, 72.0F);
    } else {
        ui().drawCentered("3DS", frame.x + frame.width * 0.5F, frame.y + 30.0F, 0.7F, LockedInk);
    }
    const float textLeft = 114.0F + eyeOffset;
    ui().drawText(text(game.name), textLeft, 52.0F, 0.74F, CardInk);
    float lineY = 80.0F;
    for (const std::string& line : wrap(text(game.kind == PluginKind::Acnl ? TextId::PluginsDescAcnl : TextId::PluginsDescLiveBox), 0.48F, 266.0F)) {
        ui().drawText(line, textLeft, lineY, 0.48F, CardMuted);
        lineY += 16.0F;
    }
    const std::string installed = tile.installed && !tile.installed->empty() ? "v" + *tile.installed : "-";
    const std::string latest = plugin != nullptr && plugin->available ? "v" + plugin->version : "-";
    const std::array<std::pair<TextId, std::string>, 2> columns = {{{TextId::PluginsColumnInstalled, installed}, {TextId::PluginsColumnLatest, latest}}};
    for (std::size_t column = 0; column < columns.size(); ++column) {
        const UiRect card{18.0F + eyeOffset + static_cast<float>(column) * 186.0F, 146.0F, 178.0F, 46.0F};
        drawCard(card, 1.0F, CardBorder, CardFill);
        ui().drawText(text(columns[column].first), card.x + 12.0F, card.y + 4.0F, 0.44F, CardMuted);
        ui().drawRight(columns[column].second, card.x + card.width - 12.0F, card.y + 14.0F, 0.66F, CardInk);
    }
    TextId status = TextId::PluginsUnavailable;
    u32 color = LockedInk;
    if (!catalog_.success) {
        status = TextId::PluginsOffline;
        color = Error;
    } else if (plugin != nullptr && !plugin->access) {
        status = TextId::PluginsLocked;
    } else if (plugin != nullptr && plugin->available) {
        const bool current = tile.installed && *tile.installed == plugin->version;
        status = current ? TextId::PluginsUpToDate : TextId::PluginsUpdateAvailable;
        color = current ? StatusGreen : WarmBrand;
    }
    C2D_DrawRectSolid(textLeft, 116.0F, 0.05F, 3.0F, 16.0F, color);
    ui().drawText(text(status), textLeft + 10.0F, 114.0F, 0.48F, color);
    C2D_DrawRectSolid(18.0F, 202.0F, 0.05F, 364.0F, 1.0F, CardBorder);
    const std::size_t selected = selectedCount();
    ui().drawText(selected == 0 ? std::string(text(TextId::PluginsNoneSelected)) : format(TextId::PluginsSelected, {std::to_string(selected)}),
                  18.0F, 211.0F, 0.48F, selected == 0 ? CardMuted : CardInk);
    ui().drawRight(text(TextId::PluginsHint), 382.0F, 211.0F, 0.48F, CardMuted);
}

void PluginScreen::drawInstallTop(float eyeOffset) {
    const std::size_t index = std::min<std::size_t>(currentJob_.load(), jobKinds_.empty() ? 0 : jobKinds_.size() - 1);
    const std::uint32_t total = progress_.total.load();
    const std::uint32_t received = progress_.received.load();
    if (!jobKinds_.empty()) {
        ui().drawCentered(pluginName(jobKinds_[index]), 200.0F + eyeOffset, 60.0F, 0.74F, CardInk);
        ui().drawCentered(format(TextId::PluginsStep, {std::to_string(index + 1), std::to_string(jobKinds_.size())}), 200.0F, 90.0F, 0.5F, CardMuted);
    }
    const TextId headline = total == 0 ? TextId::PluginConnecting : received < total ? TextId::PluginDownloading : TextId::PluginInstalling;
    ui().drawCentered(text(headline), 200.0F + eyeOffset, 118.0F, 0.5F, WarmBrand);
    const float share = total == 0 ? 0.0F : static_cast<float>(std::min(received, total)) / static_cast<float>(total);
    ui().drawProgressBar({BarX, BarY, BarWidth, BarHeight}, share, TrackColor);
    if (total != 0) {
        ui().drawCentered(std::to_string(received / BytesPerKb) + " / " + std::to_string(total / BytesPerKb) + " KB", 200.0F, 166.0F, 0.5F, CardMuted);
    }
}

void PluginScreen::drawDoneTop(float eyeOffset) {
    const bool allInstalled = !run_.results.empty() && std::all_of(run_.results.begin(), run_.results.end(),
        [](const JobResult& result) { return result.outcome == PluginInstallOutcome::Installed; });
    if (allInstalled) {
        ui().drawCentered(text(TextId::PluginsDoneTitle), 200.0F + eyeOffset, 54.0F, 0.8F, StatusGreen);
    } else {
        ui().drawCentered(text(TextId::PluginFailedTitle), 200.0F + eyeOffset, 52.0F, 0.7F, Error);
        ui().drawCentered(text(TextId::PluginsDoneSome), 200.0F, 78.0F, 0.5F, CardInk);
    }
    float y = 104.0F;
    for (const JobResult& result : run_.results) {
        const bool installed = result.outcome == PluginInstallOutcome::Installed;
        const std::string line = std::string(pluginName(result.kind)) + " v" + result.version + "  -  "
            + std::string(text(installed ? TextId::PluginsResultInstalled : TextId::PluginsResultFailed));
        ui().drawCentered(line, 200.0F, y, 0.46F, installed ? CardInk : Error);
        y += 20.0F;
    }
    y += 8.0F;
    const auto steps = [&](auto list, u32 color) {
        for (const TextId step : list) {
            ui().drawCentered(text(step), 200.0F, y, 0.48F, color);
            y += 18.0F;
        }
    };
    if (!allInstalled && run_.results.empty()) {
        ui().drawCentered(text(TextId::PluginFailedStorage), 200.0F, y, 0.48F, Error);
    } else if (run_.loader == PluginLoaderState::Missing) {
        steps(UpdateLumaSteps, Error);
    } else if (run_.loader == PluginLoaderState::Enabled) {
        steps(ReadySteps, CardInk);
    } else if (!std::none_of(run_.results.begin(), run_.results.end(), [](const JobResult& result) { return result.outcome == PluginInstallOutcome::Installed; })) {
        steps(NextSteps, CardInk);
    }
}

void PluginScreen::renderTop(float eyeOffset) {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.02F, 400.0F, 240.0F, PageFill);
    drawTopHeader(eyeOffset);
    switch (phase_) {
        case Phase::Loading:
            ui().drawSpinner(200.0F + eyeOffset, 100.0F, 24.0F, WarmBrand);
            ui().drawCentered(text(TextId::PluginsLoading), 200.0F + eyeOffset, 140.0F, 0.56F, CardInk);
            return;
        case Phase::Select:
            drawSelectTop(eyeOffset);
            return;
        case Phase::Installing:
            drawInstallTop(eyeOffset);
            return;
        case Phase::Done:
            drawDoneTop(eyeOffset);
            return;
    }
}

void PluginScreen::render() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.02F, 320.0F, 240.0F, PageFill);
    switch (phase_) {
        case Phase::Loading:
            ui().drawCentered(text(TextId::PluginPleaseWait), 160.0F, 104.0F, 0.6F, CardInk);
            return;
        case Phase::Select: {
            for (std::size_t slot = 0; slot < TilesPerPage; ++slot) {
                const std::size_t index = page_ * TilesPerPage + slot;
                if (index < tiles_.size()) {
                    drawTile(index, tileRect(slot));
                } else {
                    drawPlaceholder(tileRect(slot), ui());
                }
            }
            drawFooterButtons();
            return;
        }
        case Phase::Installing:
            ui().drawCentered(text(TextId::PluginPleaseWait), 160.0F, 92.0F, 0.62F, CardInk);
            ui().drawCentered(text(TextId::PluginKeepPowerOn), 160.0F, 122.0F, 0.5F, CardMuted);
            return;
        case Phase::Done:
            ui().drawCentered(text(TextId::PluginsDoneTitle), 160.0F, 84.0F, 0.62F, CardInk);
            drawCard(DoneButton, 0.0F, WarmBrand, WarmBrand);
            ui().drawCentered("A  " + std::string(text(TextId::Ok)), 160.0F, DoneButton.y + 13.0F, 0.52F, Surface);
            return;
    }
}
