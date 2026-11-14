#pragma once

#include "gui/UiRenderer.hpp"
#include "gui/icons/IconTexture.hpp"
#include "i18n/Localization.hpp"
#include "plugin/PluginSetup.hpp"

#include <cstddef>
#include <optional>
#include <string>

class PluginDetailsView {
public:
    PluginDetailsView(UiRenderer& ui, const Localization& text, const PluginSetup& setup)
        : ui_(ui), text_(text), setup_(setup) {}

    void drawHeader(float eyeOffset, const std::optional<std::string>& pageLabel);
    void drawLoading(float eyeOffset);
    void drawGame(float eyeOffset, const PluginGame& game, const std::optional<std::string>& installed,
                  const IconTexture* icon, std::size_t selectedCount);
    void drawInstalling(float eyeOffset);
    void drawResult(float eyeOffset);

private:
    void drawIconFrame(const UiRect& frame, const IconTexture* icon);
    void drawVersionCards(float eyeOffset, const std::string& installed, const std::string& latest);
    void drawStatus(float left, const CatalogPlugin* plugin, const std::optional<std::string>& installed);
    void drawNextSteps(float y);

    UiRenderer& ui_;
    const Localization& text_;
    const PluginSetup& setup_;
};
