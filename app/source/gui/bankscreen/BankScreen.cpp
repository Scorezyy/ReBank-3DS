#include "gui/bankscreen/BankScreen.hpp"
#include "app/App.hpp"
#include "core/Logger.hpp"
#include "gui/GameVisual.hpp"
#include "gui/elements/BoxBackground.hpp"
#include "gui/elements/Cursor.hpp"
#include "gui/elements/PokemonBadges.hpp"
#include "gui/elements/Shapes.hpp"
#include "gui/elements/TextMetrics.hpp"
#include "gui/Theme.hpp"

#include <enums/Species.hpp>
#include <utils/i18n.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace Gui;

void BankScreen::pollCartridgeSlot() {
    if (!session_.saveAdapter.isCartridge()) {
        cardInsertionKnown_ = false;
        return;
    }
    bool inserted = false;
    if (R_FAILED(FSUSER_CardSlotIsInserted(&inserted))) {
        return;
    }
    if (!cardInsertionKnown_) {
        cardInsertionKnown_ = true;
        cardInserted_ = inserted;
        return;
    }
    if (inserted == cardInserted_) {
        return;
    }
    cardInserted_ = inserted;
    if (inserted) {
        return;
    }
    Logger::instance().warning("Cartridge removed while banking, returning to game select");
    storage_.reset();
    cardInsertionKnown_ = false;
    app_.status_.clear();
    app_.screen_ = App::Screen::GameSelect;
    app_.showError(std::string(app_.localization_.get(TextId::CartridgeRemovedTitle)),
                   std::string(app_.localization_.get(TextId::CartridgeRemovedMessage)));
}

void BankScreen::update(u32 keysDown, u32 keysHeld, circlePosition circle, touchPosition touch, bool touched) {
    pollCartridgeSlot();
    if (app_.screen_ != App::Screen::Bank) {
        return;
    }
    input_.handle(keysDown, keysHeld, circle, touch, touched);
}

void BankScreen::drawHeldPokemonPreview(float cx, float cy) const {
    if (!session_.hand.active || session_.hand.summary.species == 0 || !app_.resources_.pokemonSprites) {
        return;
    }
    const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, session_.hand.summary.species);
    constexpr float scale = 1.0F;
    const float w = image.subtex->width * scale;
    const float h = image.subtex->height * scale;
    C2D_DrawImageAt(image, std::round(cx - w * 0.5F), std::round(cy - h * 0.5F), 0.6F, nullptr, scale, scale);
}

void BankScreen::drawFocusCursor(float cx, float cy, float cursorYOffset, float radius, float height) const {
    const u32 arrowColor = session_.hand.active ? CursorGreen : CursorRed;
    drawBouncingCursor(cx, cy - cursorYOffset, radius, height, arrowColor);
    drawHeldPokemonPreview(cx, cy);
}

void BankScreen::onGameOpened() {
    storage_.initializeFromOpenedGame(app_.saveLoadService_.openGameResult);
    app_.screen_ = App::Screen::Bank;
}

void BankScreen::renderTop(float eyeOffset) {
    constexpr double transitionSeconds = 0.4;
    const double now = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
    const double since = now - static_cast<double>(session_.trashTransitionStart) / SYSCLOCK_ARM11;
    const float step = static_cast<float>(std::clamp(since / transitionSeconds, 0.0, 1.0));
    const float trashProgress = session_.trashBoxActive ? step : (1.0F - step);
    drawBoxBackground(app_.resources_.boxBackground, true, trashProgress);
    renderTopHeader();
    renderTopBoxGrid(eyeOffset);
    renderTopInfoPanel();
}

void BankScreen::renderTopHeader() {
    constexpr float topScale = 400.0F / 320.0F;
    constexpr float nameBarW = 200.0F * topScale;
    constexpr float nameBarY = 6.0F;

    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 8.0F, nameBarY, 0.14F,
                        nullptr, topScale, 1.0F);
    } else {
        drawPill(8.0F, nameBarY, nameBarW, 30.0F, 0.14F, BoxPlateBorder);
        drawPill(10.0F, nameBarY + 1.0F, nameBarW - 4.0F, 27.0F, 0.15F, BoxPlate);
    }
    if (session_.cloudNameFocused) {
        C2D_DrawRectSolid(8.0F, nameBarY - 1.0F, 0.145F, nameBarW, 2.0F, CursorGreen);
        C2D_DrawRectSolid(8.0F, nameBarY + 25.0F, 0.145F, nameBarW, 2.0F, CursorGreen);
        drawBouncingCursor(8.0F + nameBarW * 0.5F, nameBarY - 6.0F, 3.5F, 12.0F, CursorRed);
    }
    const auto cachedCloudName = session_.cloudBoxNames.find(static_cast<std::uint16_t>(session_.cloudBox + 1));
    const std::string cloudBoxLabel = session_.trashBoxActive
        ? std::string(app_.localization_.get(TextId::TrashCan))
        : (cachedCloudName != session_.cloudBoxNames.end()
            ? cachedCloudName->second
            : "Bank " + std::to_string(session_.cloudBox + 1));
    app_.drawCentered(cloudBoxLabel, 8.0F + nameBarW * 0.5F, nameBarY + 5.0F, 0.55F, HeaderInk);
    if (session_.trashBoxActive) {
        return;
    }
    const bool waitingOnHeldPickup = session_.hand.active && session_.hand.source == HandSource::Cloud
        && !session_.hand.payloadKnown
        && app_.loadService_.operation() == LoadService::Operation::PickupCloud;
    if (app_.loadService_.running()
        && ((app_.loadService_.operation() == LoadService::Operation::CloudBox
             && app_.loadService_.cloudBoxKey == static_cast<std::uint16_t>(session_.cloudBox))
            || waitingOnHeldPickup
            || app_.loadService_.operation() == LoadService::Operation::SwapCloud)) {
        const double seconds = static_cast<double>(svcGetSystemTick()) / SYSCLOCK_ARM11;
        const float pulse = 0.45F + 0.55F * std::sin(static_cast<float>(seconds) * 6.0F);
        C2D_DrawCircleSolid(312.0F, nameBarY + 15.0F, 0.4F, 3.0F + pulse * 2.0F, HeaderInk);
        app_.drawText("Loading", 320.0F, nameBarY + 9.0F, 0.34F, HeaderInk);
    }
}

void BankScreen::renderTopBoxGrid(float eyeOffset) {
    constexpr float topScale = 400.0F / 320.0F;
    constexpr float pitchX = 34.0F * topScale;
    constexpr float pitchY = 30.0F;
    constexpr float gridLeft = 8.0F;
    constexpr float gridTop = 46.0F;
    drawPlusMark(gridLeft - 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft - 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);

    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float cx = gridLeft + (static_cast<float>(slot % 6) + 0.5F) * pitchX;
        const float cy = gridTop + (static_cast<float>(slot / 6) + 0.5F) * pitchY;
        const PokemonSummary& pokemon = session_.trashBoxActive
            ? session_.trashBox.summaries()[slot]
            : session_.cloudPreview[slot];
        if (app_.resources_.pokemonSprites && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, pokemon.species);
            constexpr float scale = 1.0F;
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            const float spriteX = std::round(cx - w * 0.5F + eyeOffset * 0.2F);
            const float spriteY = std::round(cy - h * 0.5F);
            const std::uint8_t saveGen = session_.saveAdapter.gameGeneration();
            const std::uint8_t monFormat = pokemon.format != 0
                ? pokemon.format
                : pokemonFormatFromCode(pokemon.gameCode);
            const bool incompatible = saveGen != 0 && monFormat != 0
                && monFormat > saveGen;
            C2D_ImageTint tint{};
            C2D_PlainImageTint(&tint, C2D_Color32(72, 72, 72, 255), 0.82F);
            C2D_DrawImageAt(image, spriteX, spriteY, 0.3F,
                            incompatible ? &tint : nullptr, scale, scale);
            drawPokemonBadges(app_.resources_.overlayIcons, pokemon, cx, cy, w * 0.5F, h * 0.5F, 0.31F);
        }
        if (session_.storagePane == StoragePane::Cloud && slot == session_.focusedSlot && !session_.cloudNameFocused) {
            drawFocusCursor(cx, cy, 22.0F, 3.5F, 12.0F);
        }
    }
}

void BankScreen::renderTopInfoPanel() {
    constexpr float topScale = 400.0F / 320.0F;
    constexpr float pitchX = 34.0F * topScale;
    constexpr float gridLeft = 8.0F;
    const float gridRight = gridLeft + pitchX * 6.0F;
    const float infoCenterX = gridRight + (400.0F - gridRight) * 0.5F;
    constexpr float infoTop = 8.0F;
    constexpr auto lang = pksm::Language::ENG;

    const PokemonSummary& focused = session_.storagePane == StoragePane::Cloud
        ? (session_.trashBoxActive
            ? session_.trashBox.summaries()[session_.focusedSlot]
            : session_.cloudPreview[session_.focusedSlot])
        : (session_.storagePane == StoragePane::Party
            ? session_.partyWorking.summaries[session_.focusedSlot]
            : session_.storage.pokemon(session_.focusedSlot));
    if (focused.species != 0) {
        const std::string speciesName = pksm::Species(focused.species).localize(lang);
        const std::string header = "#" + std::to_string(focused.species) + " "
            + (speciesName.empty() ? focused.nickname : speciesName);
        if (app_.resources_.nameDexPlate) {
            constexpr float plateScaleY = 0.85F;
            const C2D_Image plateImage = C2D_SpriteSheetGetImage(app_.resources_.nameDexPlate, 0);
            const float plateW = 400.0F - gridRight;
            const float plateScaleX = plateW / static_cast<float>(plateImage.subtex->width);
            C2D_DrawImageAt(plateImage, gridRight, infoTop - 4.0F, 0.13F,
                            nullptr, plateScaleX, plateScaleY);
            app_.drawCentered(header, infoCenterX, infoTop, 0.46F,
                              C2D_Color32(250, 247, 238, 255));
        } else {
            app_.drawCentered(header, infoCenterX, infoTop, 0.46F, HeaderInk);
        }
        {
            const C2D_SpriteSheet genderSheet = focused.gender == pksm::Gender::Male
                ? app_.resources_.genderMaleIcon
                : (focused.gender == pksm::Gender::Female
                    ? app_.resources_.genderFemaleIcon
                    : app_.resources_.genderlessIcon);
            if (genderSheet && focused.gender != pksm::Gender::Genderless) {
                const C2D_Image genderImage = C2D_SpriteSheetGetImage(genderSheet, 0);
                if (genderImage.tex) {
                    const float headerW = app_.textWidth(header, 0.46F);
                    constexpr float genderScale = 0.8F;
                    const float iconW = static_cast<float>(genderImage.subtex->width) * genderScale;
                    const float iconH = static_cast<float>(genderImage.subtex->height) * genderScale;
                    const float iconX = std::min(infoCenterX + headerW * 0.5F + 4.0F, 400.0F - 4.0F - iconW);
                    C2D_DrawImageAt(genderImage, iconX, infoTop + 9.0F - iconH * 0.5F,
                                    0.15F, nullptr, genderScale, genderScale);
                }
            }
        }
        static const std::string placeholder = "-----";
        const float colLeft = gridRight;
        const float colWidth = 400.0F - colLeft;
        const float valueRightX = 400.0F - 8.0F;
        int rowIndex = 0;
        const auto drawInfoRow = [&](float y, float height, const std::string& label,
                                      const std::string& value, float fontSize, const C2D_Image* icon) {
            if (rowIndex % 2 == 0 && app_.resources_.infoStripe) {
                const C2D_Image stripeImage = C2D_SpriteSheetGetImage(app_.resources_.infoStripe, 0);
                const float scaleX = colWidth / static_cast<float>(stripeImage.subtex->width);
                const float scaleY = height / static_cast<float>(stripeImage.subtex->height);
                C2D_DrawImageAt(stripeImage, colLeft, y, 0.12F, nullptr, scaleX, scaleY);
            }
            if (app_.resources_.pointSmall) {
                const C2D_Image dotImage = C2D_SpriteSheetGetImage(app_.resources_.pointSmall, 0);
                C2D_DrawImageAt(dotImage, colLeft + 6.0F,
                                y + height * 0.5F - static_cast<float>(dotImage.subtex->height) * 0.5F, 0.13F);
            }
            float textX = colLeft + 16.0F;
            if (icon && icon->tex) {
                const float iconH = static_cast<float>(icon->subtex->height);
                const float iconW = static_cast<float>(icon->subtex->width);
                C2D_DrawImageAt(*icon, textX, y + height * 0.5F - iconH * 0.5F, 0.14F);
                textX += iconW + 4.0F;
            }
            app_.drawText(label, textX, y + 2.0F, fontSize, HeaderInk);
            app_.drawRight(value, valueRightX, y + 2.0F, fontSize, HeaderInk);
            ++rowIndex;
        };

        constexpr float rowH = 15.0F;
        float rowY = infoTop + 18.0F;
        drawInfoRow(rowY, rowH, "Lv.", std::to_string(focused.level), 0.40F, nullptr);
        rowY += rowH;
        drawInfoRow(rowY, rowH, "OT:", focused.trainerName, 0.40F, nullptr);
        rowY += rowH;
        if (!focused.nickname.empty() && focused.nickname != speciesName) {
            drawInfoRow(rowY, rowH, "Nickname:", focused.nickname, 0.40F, nullptr);
            rowY += rowH;
        }
        {
            const std::string itemName = focused.heldItem != 0 ? i18n::item(lang, focused.heldItem) : "";
            drawInfoRow(rowY, rowH, "Item:", itemName.empty() ? placeholder : itemName, 0.38F, nullptr);
            rowY += rowH;
        }
        const std::string abilityName = i18n::ability(lang, focused.ability);
        drawInfoRow(rowY, rowH, "Ability:", abilityName.empty() ? placeholder : abilityName, 0.38F, nullptr);
        rowY += rowH;
        drawInfoRow(rowY, rowH, "Nature:", i18n::nature(lang, focused.nature), 0.38F, nullptr);
        rowY += rowH;
        if (focused.language != pksm::Language::None) {
            drawInfoRow(rowY, rowH, "Lang:", i18n::langString(focused.language), 0.34F, nullptr);
            rowY += rowH;
        }

        const float typeNativeH = typeBannerHeight(app_.resources_.typeBanners, focused.type1);
        constexpr float typeTargetH = 12.0F;
        const float typeScale = typeNativeH > 0.0F ? typeTargetH / typeNativeH : 1.0F;
        const float bannerW1 = typeBannerWidth(app_.resources_.typeBanners, focused.type1, typeScale);
        const float bannerH1 = typeNativeH * typeScale;
        float bannerY = rowY;
        if (rowIndex % 2 == 0 && app_.resources_.infoStripe) {
            const C2D_Image stripeImage = C2D_SpriteSheetGetImage(app_.resources_.infoStripe, 0);
            const float scaleX = colWidth / static_cast<float>(stripeImage.subtex->width);
            const float scaleY = rowH / static_cast<float>(stripeImage.subtex->height);
            C2D_DrawImageAt(stripeImage, colLeft, bannerY, 0.12F, nullptr, scaleX, scaleY);
        }
        ++rowIndex;
        app_.drawText("Type", colLeft + 16.0F, bannerY + 2.0F, 0.40F, HeaderInk);
        const float bannerCenterY = bannerY + (rowH - bannerH1) * 0.5F;
        if (focused.type1 == focused.type2) {
            drawTypeBanner(app_.resources_.typeBanners, focused.type1,
                           valueRightX - bannerW1, bannerCenterY, 0.32F, typeScale);
        } else {
            const float bannerW2 = typeBannerWidth(app_.resources_.typeBanners, focused.type2, typeScale);
            constexpr float bannerGap = 4.0F;
            const float pairLeft = valueRightX - (bannerW1 + bannerGap + bannerW2);
            drawTypeBanner(app_.resources_.typeBanners, focused.type1, pairLeft, bannerCenterY, 0.32F, typeScale);
            drawTypeBanner(app_.resources_.typeBanners, focused.type2,
                           pairLeft + bannerW1 + bannerGap, bannerCenterY, 0.32F, typeScale);
        }
        bannerY += rowH;

        const std::string originName = i18n::game(lang, focused.originGame);
        if (!originName.empty()) {
            drawInfoRow(bannerY, rowH, "Origin:", originName, 0.40F, nullptr);
            bannerY += rowH;
        }

        bannerY += 4.0F;
        for (const pksm::Move& move : focused.moves) {
            const std::string moveName = move == pksm::Move::None ? "" : i18n::move(lang, move);
            drawInfoRow(bannerY, 15.0F, "Move:", moveName.empty() ? placeholder : moveName, 0.38F, nullptr);
            bannerY += 16.0F;
        }
    } else {
        const std::string emptyLabel = session_.storagePane == StoragePane::Cloud
            ? "Empty cloud slot"
            : (session_.storagePane == StoragePane::Party ? "Empty party slot" : "Empty slot");
        app_.drawCentered(emptyLabel, infoCenterX, infoTop + 16.0F, 0.4F, HeaderInk);
    }
}

void BankScreen::render() {
    if (session_.errorDialogVisible) {
        renderErrorDialog();
        return;
    }
    if (session_.trashConfirmVisible) {
        renderTrashConfirmDialog();
        return;
    }
    renderStorageBottom();
}

void BankScreen::renderStorageBottom() {
    drawLinePattern(app_.resources_.bottomBackground, C2D_Color32(158, 224, 152, 255), false);
    renderStatusBar();
    renderLocalBoxHeader();
    renderLocalGrid();
    renderTeamHeader();
    renderPartyGrid();
    if (commit_.running()) {
        renderCommitOverlay();
        return;
    }
    renderActionHints();
}

void BankScreen::renderStatusBar() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.05F, 320.0F, 20.0F, C2D_Color32(215, 232, 224, 235));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.1F, 7.0F, C2D_Color32(210, 40, 40, 255));
    C2D_DrawRectSolid(7.0F, 9.0F, 0.15F, 14.0F, 2.0F, C2D_Color32(30, 30, 30, 255));
    C2D_DrawCircleSolid(14.0F, 10.0F, 0.2F, 2.5F, C2D_Color32(240, 240, 240, 255));
    const bool fetching = app_.loadService_.running()
        && ((session_.hand.active && session_.hand.source == HandSource::Cloud && !session_.hand.payloadKnown
             && app_.loadService_.operation() == LoadService::Operation::PickupCloud)
            || app_.loadService_.operation() == LoadService::Operation::SwapCloud);
    app_.drawText(fetching ? "FETCHING" : (session_.hand.active ? "HOLDING" : "READY"),
             30.0F, 6.0F, 0.34F, fetching ? CursorGreen : (session_.hand.active ? CursorGreen : HeaderInk));
    if (storage_.hasPendingChanges()) {
        app_.drawText("PENDING", 100.0F, 6.0F, 0.34F, CursorGreen);
    }
    C2D_DrawRectSolid(252.0F, 2.0F, 0.1F, 60.0F, 16.0F, C2D_Color32(58, 58, 58, 255));
    app_.drawCentered("START", 282.0F, 5.0F, 0.4F, C2D_Color32(240, 240, 240, 255));
}

void BankScreen::renderLocalBoxHeader() {
    if (app_.resources_.boxNameBarSheet) {
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(app_.resources_.boxNameBarSheet, 0), 6.0F, 26.0F, 0.14F);
    } else {
        drawPill(6.0F, 26.0F, 200.0F, 26.0F, 0.14F, BoxPlateBorder);
        drawPill(8.0F, 27.0F, 196.0F, 23.0F, 0.15F, BoxPlate);
    }
    const std::string boxLabel = session_.localBoxName.empty()
        ? "BOX " + std::to_string(session_.localBox + 1)
        : session_.localBoxName;
    app_.drawCentered(boxLabel, 106.0F, 31.0F, 0.55F, HeaderInk);
}

void BankScreen::renderLocalGrid() {
    constexpr float pitchX = 34.0F;
    constexpr float pitchY = 30.0F;
    constexpr float gridLeft = 8.0F;
    constexpr float gridTop = 58.0F;
    drawPlusMark(gridLeft - 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop - 3.0F, HeaderInk);
    drawPlusMark(gridLeft - 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);
    drawPlusMark(gridLeft + pitchX * 6.0F + 3.0F, gridTop + pitchY * 5.0F + 3.0F, HeaderInk);

    for (std::size_t slot = 0; slot < 30; ++slot) {
        const float cx = gridLeft + (static_cast<float>(slot % 6) + 0.5F) * pitchX;
        const float cy = gridTop + (static_cast<float>(slot / 6) + 0.5F) * pitchY;
        const PokemonSummary& pokemon = session_.storage.pokemon(slot);
        const bool isSelected = session_.storage.selected(slot);
        if (app_.resources_.pokemonSprites && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, pokemon.species);
            constexpr float scale = 1.0F;
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            C2D_DrawImageAt(image, std::round(cx - w * 0.5F), std::round(cy - h * 0.5F),
                            0.3F, nullptr, scale, scale);
            if (isSelected) {
                const float half = 15.0F;
                C2D_DrawRectSolid(cx - half, cy - half, 0.35F, half * 2.0F, half * 2.0F,
                                  C2D_Color32(255, 255, 255, 160));
            }
            drawPokemonBadges(app_.resources_.overlayIcons, pokemon, cx, cy, w * 0.5F, h * 0.5F, 0.36F);
        }
        if (session_.storagePane == StoragePane::Local && slot == session_.focusedSlot) {
            drawFocusCursor(cx, cy, 20.0F, 3.0F, 10.0F);
        }
    }
}

void BankScreen::renderTeamHeader() {
    constexpr float teamHeaderX = 218.0F;
    constexpr float teamHeaderY = 26.0F;
    constexpr float teamHeaderW = 96.0F;
    constexpr float teamHeaderH = 26.0F;
    constexpr float teamHeaderBgW = 320.0F - teamHeaderX;
    if (app_.resources_.teamBackground) {
        const C2D_Image headerBg = C2D_SpriteSheetGetImage(app_.resources_.teamBackground, 0);
        if (headerBg.subtex) {
            const float scaleX = teamHeaderBgW / static_cast<float>(headerBg.subtex->width);
            const float scaleY = teamHeaderH / static_cast<float>(headerBg.subtex->height);
            C2D_DrawImageAt(headerBg, teamHeaderX, teamHeaderY, 0.14F, nullptr, scaleX, scaleY);
        }
    } else {
        drawPill(teamHeaderX, teamHeaderY, teamHeaderW, teamHeaderH, 0.14F, BoxPlateBorder);
        drawPill(teamHeaderX + 2.0F, teamHeaderY + 1.0F, teamHeaderW - 4.0F, teamHeaderH - 3.0F, 0.15F, BoxPlate);
    }
    app_.drawCentered("TEAM", teamHeaderX + teamHeaderW * 0.5F, 31.0F, 0.55F,
                      app_.resources_.teamBackground ? C2D_Color32(255, 255, 255, 255) : HeaderInk);
}

void BankScreen::renderPartyGrid() {
    constexpr float partyColAX = 244.0F;
    constexpr float partyColBX = 288.0F;
    constexpr float partyRowStep = 45.0F;
    constexpr float partyColATop = 86.0F;
    constexpr float partyColBTop = 108.0F;
    const int partyCount = session_.partyMemberCount();
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const std::size_t column = slot % 2;
        const std::size_t row = slot / 2;
        const float cx = column == 0 ? partyColAX : partyColBX;
        const float cy = (column == 0 ? partyColATop : partyColBTop) + static_cast<float>(row) * partyRowStep;
        constexpr float tileSize = 34.0F;
        drawRoundedRect(cx - tileSize * 0.5F, cy - tileSize * 0.5F, tileSize, tileSize, 8.0F, 0.10F, CursorGreen);
        drawRoundedRect(cx - tileSize * 0.5F + 2.0F, cy - tileSize * 0.5F + 2.0F, tileSize - 4.0F, tileSize - 4.0F,
                        7.0F, 0.11F, BoxPlate);
        const PokemonSummary& pokemon = session_.partyWorking.summaries[slot];

        const bool isLastMember = pokemon.species != 0 && partyCount <= 1;
        if (app_.resources_.pokemonSprites && pokemon.species != 0) {
            const C2D_Image image = C2D_SpriteSheetGetImage(app_.resources_.pokemonSprites, pokemon.species);
            constexpr float scale = 1.0F;
            const float w = image.subtex->width * scale;
            const float h = image.subtex->height * scale;
            C2D_ImageTint lockedTint{};
            C2D_PlainImageTint(&lockedTint, C2D_Color32(72, 72, 72, 255), 0.82F);
            C2D_DrawImageAt(image, std::round(cx - w * 0.5F), std::round(cy - h * 0.5F),
                            0.3F, isLastMember ? &lockedTint : nullptr, scale, scale);
            drawPokemonBadges(app_.resources_.overlayIcons, pokemon, cx, cy, w * 0.5F, h * 0.5F, 0.36F);
        }
        if (session_.storagePane == StoragePane::Party && slot == session_.focusedSlot) {
            drawFocusCursor(cx, cy, 20.0F, 3.0F, 10.0F);
        }
    }
}

void BankScreen::renderCommitOverlay() {
    C2D_DrawRectSolid(0.0F, 205.0F, 0.7F, 200.0F, 35.0F, C2D_Color32(0, 0, 0, 170));
    const int phase = commit_.phase();
    std::string label = phase == 3 ? "Writing save..."
                       : phase == 2 ? "Uploading cloud..."
                       : phase == 1 ? "Removing cloud..."
                       : "Preparing...";
    const int progress = commit_.progress();
    label += " " + std::to_string(progress) + "%";
    app_.drawCentered(label,
                 100.0F, 210.0F, 0.42F, C2D_Color32(255, 255, 255, 255));
    C2D_DrawRectSolid(10.0F, 228.0F, 0.9F, 180.0F, 6.0F, C2D_Color32(50, 50, 50, 220));
    const float fill = 180.0F * static_cast<float>(progress) / 100.0F;
    C2D_DrawRectSolid(10.0F, 228.0F, 0.92F, fill, 6.0F, CursorGreen);
}

void BankScreen::renderActionHints() {
    const bool held = session_.hand.active;
    const bool pending = storage_.hasPendingChanges();

    C2D_DrawRectSolid(0.0F, 210.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, 25));
    C2D_DrawRectSolid(0.0F, 216.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, 45));
    C2D_DrawRectSolid(0.0F, 222.0F, 0.06F, 320.0F, 6.0F, C2D_Color32(255, 255, 255, 65));
    C2D_DrawRectSolid(0.0F, 228.0F, 0.06F, 320.0F, 12.0F, C2D_Color32(255, 255, 255, 85));

    const u32 divider = C2D_Color32(20, 110, 70, 55);
    C2D_DrawRectSolid(72.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);
    C2D_DrawRectSolid(152.0F, 217.0F, 0.08F, 1.0F, 14.0F, divider);

    const u32 aGlyph = held ? CursorGreen : C2D_Color32(216, 40, 32, 150);
    app_.drawCentered("A", 16.0F, 219.0F, 0.42F, aGlyph);
    app_.drawText(held ? "Drop" : "Pick", 26.0F, 220.0F, 0.4F, HeaderInk);

    const u32 bGlyph = held ? C2D_Color32(120, 60, 160, 220) : C2D_Color32(20, 110, 70, 140);
    app_.drawCentered("B", 82.0F, 219.0F, 0.42F, bGlyph);
    app_.drawText(held ? "Return" : "Back", 92.0F, 220.0F, 0.4F, HeaderInk);

    const u32 selGlyph = pending ? CursorGreen : C2D_Color32(20, 110, 70, 140);
    app_.drawCentered("S", 162.0F, 219.0F, 0.42F, selGlyph);
    app_.drawText("Save", 172.0F, 220.0F, 0.4F, pending ? CursorGreen : HeaderInk);
}

void BankScreen::renderErrorDialog() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, C2D_Color32(12, 24, 19, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.40F, 284.0F, 208.0F, C2D_Color32(250, 247, 238, 255));
    C2D_DrawRectSolid(18.0F, 20.0F, 0.45F, 7.0F, 208.0F, Error);
    C2D_DrawRectSolid(25.0F, 20.0F, 0.45F, 277.0F, 36.0F, C2D_Color32(255, 225, 214, 255));

    app_.drawText(session_.errorDialogTitle, 36.0F, 30.0F, 0.52F, Error);
    app_.drawText(session_.errorDialogPokemon.empty() ? "Unknown Pokemon" : session_.errorDialogPokemon,
             36.0F, 68.0F, 0.62F, HeaderInk);
    if (!session_.errorDialogLocation.empty()) {
        app_.drawText(session_.errorDialogLocation, 36.0F, 90.0F, 0.38F, Muted);
    }

    std::vector<std::string> lines;
    std::string remaining = session_.errorDialogMessage.empty()
        ? "The transfer was rejected. Check rebank.log for details."
        : session_.errorDialogMessage;
    constexpr std::size_t MaxLineLength = 42;
    while (!remaining.empty() && lines.size() < 4) {
        if (remaining.size() <= MaxLineLength) {
            lines.push_back(remaining);
            break;
        }
        std::size_t split = remaining.rfind(' ', MaxLineLength);
        if (split == std::string::npos || split == 0) {
            split = MaxLineLength;
        }
        lines.push_back(remaining.substr(0, split));
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ') {
            remaining.erase(remaining.begin());
        }
    }
    if (!remaining.empty() && !lines.empty()) {
        std::string& last = lines.back();
        if (last.size() > MaxLineLength - 3) {
            last.resize(MaxLineLength - 3);
        }
        last += "...";
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        app_.drawText(lines[index], 36.0F, 116.0F + static_cast<float>(index) * 15.0F,
                 0.36F, HeaderInk);
    }

    const UiRect okButton{92.0F, 190.0F, 136.0F, 34.0F};
    C2D_DrawRectSolid(okButton.x, okButton.y, 0.46F, okButton.width, okButton.height, Brand);
    C2D_DrawRectSolid(okButton.x + 2.0F, okButton.y + 2.0F, 0.47F,
                      okButton.width - 4.0F, okButton.height - 4.0F, CursorGreen);
    app_.drawCentered("OK", 160.0F, 199.0F, 0.52F, C2D_Color32(255, 255, 255, 255));
    app_.drawCentered("A / B", 268.0F, 201.0F, 0.30F, Muted);
}

void BankScreen::renderTrashConfirmDialog() {
    C2D_DrawRectSolid(0.0F, 0.0F, 0.35F, 320.0F, 240.0F, C2D_Color32(12, 24, 19, 255));
    C2D_DrawRectSolid(18.0F, 52.0F, 0.40F, 284.0F, 148.0F, C2D_Color32(250, 247, 238, 255));

    app_.drawCentered(app_.localization_.get(TextId::TrashCan), 160.0F, 68.0F, 0.5F, HeaderInk);
    app_.drawCentered(app_.localization_.get(TextId::TrashConfirmMessage), 160.0F, 98.0F, 0.42F, HeaderInk);

    const UiRect yesButton{40.0F, 130.0F, 100.0F, 34.0F};
    C2D_DrawRectSolid(yesButton.x, yesButton.y, 0.46F, yesButton.width, yesButton.height, CursorGreen);
    app_.drawCentered(app_.localization_.get(TextId::Yes), 90.0F, 139.0F, 0.5F, C2D_Color32(255, 255, 255, 255));

    const UiRect noButton{180.0F, 130.0F, 100.0F, 34.0F};
    C2D_DrawRectSolid(noButton.x, noButton.y, 0.46F, noButton.width, noButton.height, Brand);
    app_.drawCentered(app_.localization_.get(TextId::No), 230.0F, 139.0F, 0.5F, C2D_Color32(255, 255, 255, 255));

    app_.drawCentered("A / B", 160.0F, 180.0F, 0.36F, HeaderInk);
}
