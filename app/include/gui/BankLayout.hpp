#pragma once

#include "bank/BankTypes.hpp"
#include "gui/Theme.hpp"
#include "selection/GridGeometry.hpp"

#include <cstddef>

namespace BankLayout {
inline constexpr float TopScale = 400.0F / 320.0F;

struct BoxGrid {
    float left;
    float top;
    float pitchX;
    float pitchY;
    std::size_t columns = 6;
    std::size_t rows = 5;

    constexpr std::size_t slotCount() const { return columns * rows; }
    constexpr float right() const { return left + pitchX * static_cast<float>(columns); }
    constexpr float bottom() const { return top + pitchY * static_cast<float>(rows); }
    constexpr float cellLeft(std::size_t slot) const { return left + static_cast<float>(slot % columns) * pitchX; }
    constexpr float cellTop(std::size_t slot) const { return top + static_cast<float>(slot / columns) * pitchY; }
    constexpr float centerX(std::size_t slot) const { return cellLeft(slot) + pitchX * 0.5F; }
    constexpr float centerY(std::size_t slot) const { return cellTop(slot) + pitchY * 0.5F; }
    constexpr UiRect cell(std::size_t slot) const { return {cellLeft(slot), cellTop(slot), pitchX, pitchY}; }
};

inline constexpr BoxGrid CloudGrid{8.0F, 46.0F, 34.0F * TopScale, 30.0F};
inline constexpr float LocalGridWidth = 204.0F;
inline constexpr float LocalGridHeight = 150.0F;

inline BoxGrid localGrid(const GridGeometry& geometry) {
    const auto columns = static_cast<std::size_t>(geometry.columns());
    const auto rows = static_cast<std::size_t>(geometry.rows());
    return {8.0F, 58.0F, LocalGridWidth / static_cast<float>(columns), LocalGridHeight / static_cast<float>(rows),
            columns, rows};
}

inline constexpr float PartyTileSize = 34.0F;
inline constexpr float PartyTouchHalf = 18.0F;

struct Point {
    float x;
    float y;
};

constexpr Point partyTileCenter(std::size_t slot) {
    const bool firstColumn = slot % 2 == 0;
    const float row = static_cast<float>(slot / 2);
    return {firstColumn ? 244.0F : 288.0F, (firstColumn ? 86.0F : 108.0F) + row * 45.0F};
}

constexpr UiRect partyTouchRect(std::size_t slot) {
    const Point center = partyTileCenter(slot);
    return {center.x - PartyTouchHalf, center.y - PartyTouchHalf, PartyTouchHalf * 2.0F, PartyTouchHalf * 2.0F};
}

inline constexpr UiRect TrashYesButton{40.0F, 130.0F, 100.0F, 34.0F};
inline constexpr UiRect TrashNoButton{180.0F, 130.0F, 100.0F, 34.0F};
}
