#pragma once

#include "selection/GridGeometry.hpp"

#include <cstdint>

enum class CursorDirection : std::uint8_t {
    None,
    Up,
    Down,
    Left,
    Right
};

GridPoint stepOf(CursorDirection direction);
bool isVertical(CursorDirection direction);
bool isHorizontal(CursorDirection direction);
