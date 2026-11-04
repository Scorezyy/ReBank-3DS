#include "gui/elements/PolicyReader.hpp"

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

std::size_t breakFor(const UiRenderer& ui, std::string_view word, float size, float width) {
    std::size_t length = word.size();
    while (length > 1 && ui.measure(word.substr(0, length), size) > width) {
        --length;
        while (length > 1 && (static_cast<unsigned char>(word[length]) & 0xC0) == 0x80) {
            --length;
        }
    }
    return length;
}
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
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t newline = text.find('\n', start);
        std::string_view paragraph = text.substr(start, newline == std::string_view::npos ? text.npos : newline - start);
        std::string line;
        while (!paragraph.empty()) {
            const std::size_t space = paragraph.find(' ');
            std::string_view word = paragraph.substr(0, space);
            paragraph = space == std::string_view::npos ? std::string_view{} : paragraph.substr(space + 1);
            if (word.empty()) {
                continue;
            }
            const std::string candidate = line.empty() ? std::string(word) : line + " " + std::string(word);
            if (ui.measure(candidate, size) <= width) {
                line = candidate;
                continue;
            }
            if (!line.empty()) {
                lines_.push_back({line, TextLeft + indent, contentHeight_, size, color});
                contentHeight_ += height;
            }
            while (ui.measure(word, size) > width) {
                const std::size_t cut = breakFor(ui, word, size, width);
                lines_.push_back({std::string(word.substr(0, cut)), TextLeft + indent, contentHeight_, size, color});
                contentHeight_ += height;
                word = word.substr(cut);
            }
            line = std::string(word);
        }
        if (!line.empty()) {
            lines_.push_back({line, TextLeft + indent, contentHeight_, size, color});
            contentHeight_ += height;
        }
        if (newline == std::string_view::npos) {
            break;
        }
        start = newline + 1;
    }
    contentHeight_ += spaceAfter;
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
            case PolicyBlock::Kind::Bullet: {
                const std::size_t first = lines_.size();
                addWrapped(ui, block.text, BulletIndent, 0.40F, WarmInk, 0.0F, 3.0F);
                if (first < lines_.size()) {
                    lines_.push_back({{}, TextLeft + 4.0F, lines_[first].y, 0.40F, WarmBrand, true});
                }
                break;
            }
            case PolicyBlock::Kind::Row: {
                if (block.cells.empty()) {
                    break;
                }
                addWrapped(ui, block.cells.front(), 0.0F, 0.40F, WarmBrandDark, 2.0F, 0.0F);
                std::string rest;
                for (std::size_t cell = 1; cell < block.cells.size(); ++cell) {
                    rest += (rest.empty() ? "" : " · ") + block.cells[cell];
                }
                if (!rest.empty()) {
                    addWrapped(ui, rest, BulletIndent, 0.38F, WarmMuted, 0.0F, 4.0F);
                }
                break;
            }
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

    ui.drawWarmButton(DeclineButton, decline, false);
    ui.drawWarmButton(AcceptButton, accept, true);
}
