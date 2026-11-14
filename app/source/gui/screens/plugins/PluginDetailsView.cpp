#include "gui/screens/plugins/PluginDetailsView.hpp"

#include "gui/Theme.hpp"
#include "gui/screens/plugins/PluginPalette.hpp"
#include "gui/widgets/ByteProgress.hpp"
#include "gui/widgets/Shapes.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <utility>

using namespace Gui;
using namespace PluginPalette;

namespace {
constexpr UiRect ProgressBar{60.0F, 146.0F, 280.0F, 12.0F};
constexpr std::array<TextId, 3> NextSteps = {TextId::PluginNextEnable, TextId::PluginNextKeys, TextId::PluginNextStart};
constexpr std::array<TextId, 2> ReadySteps = {TextId::PluginLoaderEnabled, TextId::PluginNextJustStart};
constexpr std::array<TextId, 3> UpdateLumaSteps = {TextId::PluginLumaTooOld, TextId::PluginLumaUpdate,
                                                   TextId::PluginLumaThenStart};

std::string_view pluginName(PluginKind kind) {
    return kind == PluginKind::Acnl ? "ReBank ACNL" : "ReBank LiveBox";
}

std::string versionLabel(const std::optional<std::string>& version) {
    return version && !version->empty() ? "v" + *version : "-";
}
}

void PluginDetailsView::drawHeader(float eyeOffset, const std::optional<std::string>& pageLabel) {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.02F, 400.0F, 240.0F, PageFill);
    ui_.drawText(text_.get(TextId::PluginsTitle), 18.0F + eyeOffset, 12.0F, 0.66F, CardInk);
    if (pageLabel) {
        ui_.drawRight(*pageLabel, 382.0F + eyeOffset, 15.0F, 0.5F, CardMuted);
    }
    C2D_DrawRectSolid(18.0F + eyeOffset, 40.0F, 0.05F, 364.0F, 1.0F, CardBorder);
}

void PluginDetailsView::drawLoading(float eyeOffset) {
    ui_.drawSpinner(200.0F + eyeOffset, 100.0F, 24.0F, WarmBrand);
    ui_.drawCentered(text_.get(TextId::PluginsLoading), 200.0F + eyeOffset, 140.0F, 0.56F, CardInk);
}

void PluginDetailsView::drawGame(float eyeOffset, const PluginGame& game, const std::optional<std::string>& installed,
                                 const IconTexture* icon, std::size_t selectedCount) {
    const CatalogPlugin* plugin = setup_.find(game.kind);
    drawIconFrame({18.0F + eyeOffset, 52.0F, 84.0F, 84.0F}, icon);

    const float textLeft = 114.0F + eyeOffset;
    ui_.drawText(text_.get(game.name), textLeft, 52.0F, 0.74F, CardInk);
    const TextId description = game.kind == PluginKind::Acnl ? TextId::PluginsDescAcnl : TextId::PluginsDescLiveBox;
    float lineY = 80.0F;
    for (const std::string& line : ui_.wrap(text_.get(description), 0.48F, 266.0F)) {
        ui_.drawText(line, textLeft, lineY, 0.48F, CardMuted);
        lineY += 16.0F;
    }
    const std::optional<std::string> latest = plugin != nullptr && plugin->available
        ? std::optional<std::string>(plugin->version)
        : std::nullopt;
    drawVersionCards(eyeOffset, versionLabel(installed), versionLabel(latest));
    drawStatus(textLeft, plugin, installed);

    C2D_DrawRectSolid(18.0F, 202.0F, 0.05F, 364.0F, 1.0F, CardBorder);
    const std::string selection = selectedCount == 0
        ? std::string(text_.get(TextId::PluginsNoneSelected))
        : text_.format(TextId::PluginsSelected, {std::to_string(selectedCount)});
    ui_.drawText(selection, 18.0F, 211.0F, 0.48F, selectedCount == 0 ? CardMuted : CardInk);
    ui_.drawRight(text_.get(TextId::PluginsHint), 382.0F, 211.0F, 0.48F, CardMuted);
}

void PluginDetailsView::drawIconFrame(const UiRect& frame, const IconTexture* icon) {
    const bool loaded = icon != nullptr && icon->loaded();
    drawCard(frame, CornerRadius, 1.0F, CardBorder, loaded ? CardFill : LockedFill);
    if (loaded) {
        icon->draw(frame.x + 6.0F, frame.y + 6.0F, 0.2F, 72.0F);
        return;
    }
    ui_.drawCentered("3DS", frame.x + frame.width * 0.5F, frame.y + 30.0F, 0.7F, LockedInk);
}

void PluginDetailsView::drawVersionCards(float eyeOffset, const std::string& installed, const std::string& latest) {
    const std::array<std::pair<TextId, const std::string*>, 2> columns = {
        {{TextId::PluginsColumnInstalled, &installed}, {TextId::PluginsColumnLatest, &latest}}};
    for (std::size_t column = 0; column < columns.size(); ++column) {
        const UiRect card{18.0F + eyeOffset + static_cast<float>(column) * 186.0F, 146.0F, 178.0F, 46.0F};
        drawCard(card, CornerRadius, 1.0F, CardBorder, CardFill);
        ui_.drawText(text_.get(columns[column].first), card.x + 12.0F, card.y + 4.0F, 0.44F, CardMuted);
        ui_.drawRight(*columns[column].second, card.x + card.width - 12.0F, card.y + 14.0F, 0.66F, CardInk);
    }
}

void PluginDetailsView::drawStatus(float left, const CatalogPlugin* plugin,
                                   const std::optional<std::string>& installed) {
    TextId status = TextId::PluginsUnavailable;
    u32 color = LockedInk;
    if (!setup_.catalogLoaded()) {
        status = TextId::PluginsOffline;
        color = Error;
    } else if (plugin != nullptr && !plugin->access) {
        status = TextId::PluginsLocked;
    } else if (plugin != nullptr && plugin->available) {
        const bool current = installed && *installed == plugin->version;
        status = current ? TextId::PluginsUpToDate : TextId::PluginsUpdateAvailable;
        color = current ? StatusGreen : WarmBrand;
    }
    C2D_DrawRectSolid(left, 116.0F, 0.05F, 3.0F, 16.0F, color);
    ui_.drawText(text_.get(status), left + 10.0F, 114.0F, 0.48F, color);
}

void PluginDetailsView::drawInstalling(float eyeOffset) {
    const std::vector<PluginKind>& kinds = setup_.jobKinds();
    const std::uint32_t total = setup_.total();
    const std::uint32_t received = setup_.received();
    if (!kinds.empty()) {
        const std::size_t index = std::min(setup_.currentJob(), kinds.size() - 1);
        ui_.drawCentered(pluginName(kinds[index]), 200.0F + eyeOffset, 60.0F, 0.74F, CardInk);
        ui_.drawCentered(text_.format(TextId::PluginsStep, {std::to_string(index + 1), std::to_string(kinds.size())}),
                         200.0F, 90.0F, 0.5F, CardMuted);
    }
    const TextId headline = total == 0 ? TextId::PluginConnecting
        : received < total             ? TextId::PluginDownloading
                                       : TextId::PluginInstalling;
    ui_.drawCentered(text_.get(headline), 200.0F + eyeOffset, 118.0F, 0.5F, WarmBrand);
    const float share = total == 0 ? 0.0F : static_cast<float>(std::min(received, total)) / static_cast<float>(total);
    ui_.drawProgressBar(ProgressBar, share, TrackColor);
    if (total != 0) {
        ui_.drawCentered(kilobyteProgress(received, total),
                         200.0F, 166.0F, 0.5F, CardMuted);
    }
}

void PluginDetailsView::drawResult(float eyeOffset) {
    const PluginInstallRun& run = setup_.lastRun();
    if (run.allInstalled()) {
        ui_.drawCentered(text_.get(TextId::PluginsDoneTitle), 200.0F + eyeOffset, 54.0F, 0.8F, StatusGreen);
    } else {
        ui_.drawCentered(text_.get(TextId::PluginFailedTitle), 200.0F + eyeOffset, 52.0F, 0.7F, Error);
        ui_.drawCentered(text_.get(TextId::PluginsDoneSome), 200.0F, 78.0F, 0.5F, CardInk);
    }
    float y = 104.0F;
    for (const PluginJobResult& result : run.results) {
        const bool installed = result.outcome == PluginInstallOutcome::Installed;
        const std::string line = std::string(pluginName(result.kind)) + " v" + result.version + "  -  "
            + std::string(text_.get(installed ? TextId::PluginsResultInstalled : TextId::PluginsResultFailed));
        ui_.drawCentered(line, 200.0F, y, 0.46F, installed ? CardInk : Error);
        y += 20.0F;
    }
    drawNextSteps(y + 8.0F);
}

void PluginDetailsView::drawNextSteps(float y) {
    const PluginInstallRun& run = setup_.lastRun();
    const auto steps = [&](std::span<const TextId> list, u32 color) {
        for (const TextId step : list) {
            ui_.drawCentered(text_.get(step), 200.0F, y, 0.48F, color);
            y += 18.0F;
        }
    };
    if (run.results.empty()) {
        ui_.drawCentered(text_.get(TextId::PluginFailedStorage), 200.0F, y, 0.48F, Error);
    } else if (run.loader == PluginLoaderState::Missing) {
        steps(UpdateLumaSteps, Error);
    } else if (run.loader == PluginLoaderState::Enabled) {
        steps(ReadySteps, CardInk);
    } else if (run.anyInstalled()) {
        steps(NextSteps, CardInk);
    }
}
