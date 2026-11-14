#include "gui/screens/bank/PokemonInfoPanel.hpp"

#include "gui/screens/bank/BankLayout.hpp"
#include "gui/widgets/PokemonBadges.hpp"
#include "gui/widgets/Shapes.hpp"

#include <enums/Species.hpp>
#include <utils/i18n.hpp>

#include <algorithm>

using namespace Gui;

namespace {
constexpr auto Lang = pksm::Language::ENG;
constexpr float Top = 8.0F;
constexpr float RowHeight = 15.0F;
constexpr float Left = BankLayout::CloudGrid.right();
constexpr float Width = 400.0F - Left;
constexpr float CenterX = Left + Width * 0.5F;
constexpr float ValueRight = 400.0F - 8.0F;
constexpr float GenderScale = 0.8F;
constexpr float TypeBannerHeight = 12.0F;
constexpr float TypeBannerGap = 4.0F;
constexpr std::string_view Placeholder = "-----";
constexpr u32 PlateInk = C2D_Color32(250, 247, 238, 255);

TextId emptySlotText(StoragePane pane) {
    switch (pane) {
        case StoragePane::Cloud:
            return TextId::EmptyCloudSlot;
        case StoragePane::Party:
            return TextId::EmptyPartySlot;
        default:
            return TextId::EmptySlot;
    }
}
}

void PokemonInfoPanel::render(const PokemonSummary& focused, StoragePane pane) {
    if (!focused.occupied()) {
        ui_.drawCentered(text_.get(emptySlotText(pane)), CenterX, Top + 16.0F, 0.4F, HeaderInk);
        return;
    }
    const std::string speciesName = pksm::Species(focused.species).localize(Lang);
    drawHeader(focused, speciesName);

    Row row{Top + 18.0F, 0};
    drawRow(row, text_.get(TextId::InfoLevel), std::to_string(focused.level), 0.40F);
    drawRow(row, text_.get(TextId::InfoTrainer), focused.trainerName, 0.40F);
    if (!focused.nickname.empty() && focused.nickname != speciesName) {
        drawRow(row, text_.get(TextId::InfoNickname), focused.nickname, 0.40F);
    }
    drawRow(row, text_.get(TextId::InfoItem), focused.heldItem != 0 ? i18n::item(Lang, focused.heldItem) : "", 0.38F);
    drawRow(row, text_.get(TextId::InfoAbility), i18n::ability(Lang, focused.ability), 0.38F);
    drawRow(row, text_.get(TextId::InfoNature), i18n::nature(Lang, focused.nature), 0.38F);
    if (focused.language != pksm::Language::None) {
        drawRow(row, text_.get(TextId::InfoLanguage), i18n::langString(focused.language), 0.34F);
    }
    drawTypeRow(row, focused);
    if (const std::string origin = i18n::game(Lang, focused.originGame); !origin.empty()) {
        drawRow(row, text_.get(TextId::InfoOrigin), origin, 0.40F);
    }
    row.y += 4.0F;
    for (const pksm::Move& move : focused.moves) {
        drawRow(row, text_.get(TextId::InfoMove), move == pksm::Move::None ? "" : i18n::move(Lang, move), 0.38F);
        row.y += 1.0F;
    }
}

void PokemonInfoPanel::drawHeader(const PokemonSummary& focused, const std::string& speciesName) {
    const std::string header = "#" + std::to_string(focused.species) + " "
        + (speciesName.empty() ? focused.nickname : speciesName);
    if (resources_.nameDexPlate) {
        const C2D_Image plate = C2D_SpriteSheetGetImage(resources_.nameDexPlate, 0);
        C2D_DrawImageAt(plate, Left, Top - 4.0F, 0.13F, nullptr, Width / static_cast<float>(plate.subtex->width), 0.85F);
    }
    ui_.drawCentered(header, CenterX, Top, 0.46F, resources_.nameDexPlate ? PlateInk : HeaderInk);

    const C2D_SpriteSheet genderSheet = focused.gender == pksm::Gender::Male
        ? resources_.genderMaleIcon
        : (focused.gender == pksm::Gender::Female ? resources_.genderFemaleIcon : nullptr);
    if (!genderSheet) {
        return;
    }
    const C2D_Image icon = C2D_SpriteSheetGetImage(genderSheet, 0);
    if (!icon.tex) {
        return;
    }
    const float iconWidth = static_cast<float>(icon.subtex->width) * GenderScale;
    const float iconHeight = static_cast<float>(icon.subtex->height) * GenderScale;
    const float iconX = std::min(CenterX + ui_.textWidth(header, 0.46F) * 0.5F + 4.0F, 400.0F - 4.0F - iconWidth);
    C2D_DrawImageAt(icon, iconX, Top + 9.0F - iconHeight * 0.5F, 0.15F, nullptr, GenderScale, GenderScale);
}

void PokemonInfoPanel::drawStripe(const Row& row) const {
    if (row.index % 2 == 0 && resources_.infoStripe) {
        drawStretched(resources_.infoStripe, Left, row.y, 0.12F, Width, RowHeight);
    }
}

void PokemonInfoPanel::drawRow(Row& row, std::string_view label, std::string_view value, float fontSize) {
    drawStripe(row);
    if (resources_.pointSmall) {
        const C2D_Image dot = C2D_SpriteSheetGetImage(resources_.pointSmall, 0);
        C2D_DrawImageAt(dot, Left + 6.0F, row.y + RowHeight * 0.5F - static_cast<float>(dot.subtex->height) * 0.5F,
                        0.13F);
    }
    ui_.drawText(label, Left + 16.0F, row.y + 2.0F, fontSize, HeaderInk);
    ui_.drawRight(value.empty() ? Placeholder : value, ValueRight, row.y + 2.0F, fontSize, HeaderInk);
    row.y += RowHeight;
    ++row.index;
}

void PokemonInfoPanel::drawTypeRow(Row& row, const PokemonSummary& focused) {
    const C2D_SpriteSheet sheet = resources_.typeBanners;
    drawStripe(row);
    ui_.drawText(text_.get(TextId::InfoType), Left + 16.0F, row.y + 2.0F, 0.40F, HeaderInk);

    const float nativeHeight = typeBannerHeight(sheet, focused.type1);
    const float scale = nativeHeight > 0.0F ? TypeBannerHeight / nativeHeight : 1.0F;
    const float firstWidth = typeBannerWidth(sheet, focused.type1, scale);
    const float bannerY = row.y + (RowHeight - nativeHeight * scale) * 0.5F;
    if (focused.type1 == focused.type2) {
        drawTypeBanner(sheet, focused.type1, ValueRight - firstWidth, bannerY, 0.32F, scale);
    } else {
        const float left = ValueRight - (firstWidth + TypeBannerGap + typeBannerWidth(sheet, focused.type2, scale));
        drawTypeBanner(sheet, focused.type1, left, bannerY, 0.32F, scale);
        drawTypeBanner(sheet, focused.type2, left + firstWidth + TypeBannerGap, bannerY, 0.32F, scale);
    }
    row.y += RowHeight;
    ++row.index;
}
