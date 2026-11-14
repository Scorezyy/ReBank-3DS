#include "gui/widgets/PolicyReader.hpp"

#include <algorithm>
#include <cstdlib>

using namespace Gui;

namespace {
constexpr float TextLeft = 14.0F;
constexpr float TextWidth = 284.0F;
constexpr float ViewTop = 38.0F;
constexpr float ViewBottom = 192.0F;
constexpr float ViewHeight = ViewBottom - ViewTop;
constexpr float ScrollbarX = 306.0F;
constexpr float KeyScrollSpeed = 5.0F;
constexpr float StickScrollFactor = 0.08F;
constexpr s16 StickDeadZone = 20;
constexpr float BulletIndent = 12.0F;
constexpr UiRect DeclineButton{12.0F, 198.0F, 142.0F, 34.0F};
constexpr UiRect AcceptButton{166.0F, 198.0F, 142.0F, 34.0F};
constexpr UiRect TextArea{0.0F, ViewTop, 320.0F, ViewHeight};
constexpr u32 PanelColor = C2D_Color32(255, 252, 247, 255);
constexpr u32 ScrollTrack = C2D_Color32(234, 200, 176, 255);
}

void PolicyReader::open(std::vector<PolicyBlock> blocks) {
    blocks_ = std::move(blocks);
    lines_.clear();
    scroll_ = 0.0F;
    lastTouchY_ = -1.0F;
    laidOut_ = false;
    open_ = true;
}

void PolicyReader::addWrapped(UiRenderer& ui, std::string_view text, float indent, float size, u32 color,
                              float spaceBefore, float spaceAfter) {
    contentHeight_ += spaceBefore;
    const float width = TextWidth - indent;
    const float height = ui.lineHeight(size);
    for (std::string& line : ui.wrap(text, size, width)) {
        lines_.push_back({std::move(line), TextLeft + indent, contentHeight_, size, color});
        contentHeight_ += height;
    }
    contentHeight_ += spaceAfter;
}

void PolicyReader::addBullet(UiRenderer& ui, std::string_view text) {
    const std::size_t first = lines_.size();
    addWrapped(ui, text, BulletIndent, 0.40F, WarmInk, 0.0F, 3.0F);
    if (first < lines_.size()) {
        lines_.push_back({{}, TextLeft + 4.0F, lines_[first].y, 0.40F, WarmBrand, true});
    }
}

void PolicyReader::addRow(UiRenderer& ui, const std::vector<std::string>& cells) {
    if (cells.empty()) {
        return;
    }
    addWrapped(ui, cells.front(), 0.0F, 0.40F, WarmBrandDark, 2.0F, 0.0F);
    std::string rest;
    for (std::size_t cell = 1; cell < cells.size(); ++cell) {
        rest += (rest.empty() ? "" : " · ") + cells[cell];
    }
    if (!rest.empty()) {
        addWrapped(ui, rest, BulletIndent, 0.38F, WarmMuted, 0.0F, 4.0F);
    }
}

void PolicyReader::layout(UiRenderer& ui) {
    lines_.clear();
    contentHeight_ = 4.0F;
    for (const PolicyBlock& block : blocks_) {
        switch (block.kind) {
            case PolicyBlock::Kind::Title:
                addWrapped(ui, block.text, 0.0F, 0.58F, WarmBrandDark, 0.0F, 6.0F);
                break;
            case PolicyBlock::Kind::Heading:
                addWrapped(ui, block.text, 0.0F, 0.50F, WarmBrand, 10.0F, 4.0F);
                break;
            case PolicyBlock::Kind::Subheading:
                addWrapped(ui, block.text, 0.0F, 0.45F, WarmBrandDark, 6.0F, 3.0F);
                break;
            case PolicyBlock::Kind::Paragraph:
                addWrapped(ui, block.text, 0.0F, 0.40F, WarmInk, 0.0F, 6.0F);
                break;
            case PolicyBlock::Kind::Bullet:
                addBullet(ui, block.text);
                break;
            case PolicyBlock::Kind::Row:
                addRow(ui, block.cells);
                break;
        }
    }
    contentHeight_ += 6.0F;
    laidOut_ = true;
}

float PolicyReader::maxScroll() const {
    return std::max(0.0F, contentHeight_ - ViewHeight);
}

PolicyReader::Outcome PolicyReader::update(const InputFrame& input) {
    if (!open_) {
        return Outcome::None;
    }
    if (input.pressed(KEY_A) || input.tapped(AcceptButton)) {
        open_ = false;
        return Outcome::Accepted;
    }
    if (input.pressed(KEY_B) || input.tapped(DeclineButton)) {
        open_ = false;
        return Outcome::Declined;
    }
    if (input.held & KEY_DOWN) {
        scroll_ += KeyScrollSpeed;
    }
    if (input.held & KEY_UP) {
        scroll_ -= KeyScrollSpeed;
    }
    if (std::abs(input.circle.dy) > StickDeadZone) {
        scroll_ -= static_cast<float>(input.circle.dy) * StickScrollFactor;
    }
    if (input.pressed(KEY_R)) {
        scroll_ += ViewHeight;
    }
    if (input.pressed(KEY_L)) {
        scroll_ -= ViewHeight;
    }
    if ((input.held & KEY_TOUCH) && (lastTouchY_ >= 0.0F || TextArea.contains(input.touch))) {
        const auto touchY = static_cast<float>(input.touch.py);
        if (lastTouchY_ >= 0.0F) {
            scroll_ += lastTouchY_ - touchY;
        }
        lastTouchY_ = touchY;
    } else {
        lastTouchY_ = -1.0F;
    }
    scroll_ = std::clamp(scroll_, 0.0F, maxScroll());
    return Outcome::None;
}

void PolicyReader::render(UiRenderer& ui, std::string_view title, std::string_view decline, std::string_view accept) {
    if (!laidOut_) {
        layout(ui);
        scroll_ = std::clamp(scroll_, 0.0F, maxScroll());
    }
    ui.drawWarmBackground();
    ui.drawCentered(title, 160.0F, 9.0F, 0.58F, WarmInk);
    C2D_DrawRectSolid(122.0F, 31.0F, 0.05F, 76.0F, 2.0F, WarmBrand);

    C2D_DrawRectSolid(6.0F, ViewTop - 3.0F, 0.05F, 308.0F, ViewHeight + 6.0F, PanelColor);
    for (const Line& line : lines_) {
        const float y = ViewTop + line.y - scroll_;
        if (y < ViewTop - 1.0F || y + ui.lineHeight(line.size) > ViewBottom + 1.0F) {
            continue;
        }
        if (line.dot) {
            C2D_DrawCircleSolid(line.x, y + ui.lineHeight(line.size) * 0.5F, 0.1F, 2.0F, line.color);
        } else {
            ui.drawText(line.text, line.x, y, line.size, line.color);
        }
    }

    if (maxScroll() > 0.0F) {
        const float thumb = std::max(18.0F, ViewHeight * ViewHeight / contentHeight_);
        const float thumbY = ViewTop + (ViewHeight - thumb) * (scroll_ / maxScroll());
        C2D_DrawRectSolid(ScrollbarX, ViewTop, 0.06F, 3.0F, ViewHeight, ScrollTrack);
        C2D_DrawRectSolid(ScrollbarX - 1.0F, thumbY, 0.07F, 5.0F, thumb, WarmBrand);
    }

    ui.drawButton(DeclineButton, decline, false);
    ui.drawButton(AcceptButton, accept, true);
}
