#pragma once

#include "bank/BankTypes.hpp"
#include "gui/GfxResources.hpp"
#include "gui/UiRenderer.hpp"
#include "i18n/Localization.hpp"
#include "save/pokemon/PokemonData.hpp"

#include <string>
#include <string_view>

class PokemonInfoPanel {
public:
    PokemonInfoPanel(UiRenderer& ui, const GfxResources& resources, const Localization& text)
        : ui_(ui), resources_(resources), text_(text) {}

    void render(const PokemonSummary& focused, StoragePane pane);

private:
    struct Row {
        float y;
        int index;
    };

    void drawHeader(const PokemonSummary& focused, const std::string& speciesName);
    void drawStripe(const Row& row) const;
    void drawRow(Row& row, std::string_view label, std::string_view value, float fontSize);
    void drawTypeRow(Row& row, const PokemonSummary& focused);

    UiRenderer& ui_;
    const GfxResources& resources_;
    const Localization& text_;
};
