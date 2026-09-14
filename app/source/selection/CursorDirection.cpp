#include "selection/CursorDirection.hpp"

GridPoint stepOf(CursorDirection direction) {
    switch (direction) {
        case CursorDirection::Up:
            return GridPoint{-1, 0};
        case CursorDirection::Down:
            return GridPoint{1, 0};
        case CursorDirection::Left:
            return GridPoint{0, -1};
        case CursorDirection::Right:
            return GridPoint{0, 1};
        case CursorDirection::None:
            break;
    }
    return GridPoint{0, 0};
}

bool isVertical(CursorDirection direction) {
    return direction == CursorDirection::Up || direction == CursorDirection::Down;
}

bool isHorizontal(CursorDirection direction) {
    return direction == CursorDirection::Left || direction == CursorDirection::Right;
}
