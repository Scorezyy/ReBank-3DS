#include "selection/GridGeometry.hpp"

#include <algorithm>

GridGeometry GridGeometry::forPane(StoragePane pane) {
    if (pane == StoragePane::Party) {
        return GridGeometry{2, 3};
    }
    return GridGeometry{6, 5};
}

GridPoint GridGeometry::pointOf(std::size_t slot) const {
    const int index = static_cast<int>(slot);
    return GridPoint{index / columns_, index % columns_};
}

std::size_t GridGeometry::slotOf(GridPoint point) const {
    return static_cast<std::size_t>(point.row * columns_ + point.column);
}

bool GridGeometry::contains(GridPoint point) const {
    return point.row >= 0 && point.row < rows_ && point.column >= 0 && point.column < columns_;
}

GridPoint GridGeometry::clampToGrid(GridPoint point, GridPoint minOffset, GridPoint maxOffset) const {
    GridPoint clamped = point;
    clamped.row = std::clamp(clamped.row, -minOffset.row, rows_ - 1 - maxOffset.row);
    clamped.column = std::clamp(clamped.column, -minOffset.column, columns_ - 1 - maxOffset.column);
    return clamped;
}

std::vector<std::size_t> GridGeometry::rowSlots(int row) const {
    std::vector<std::size_t> slots;
    slots.reserve(static_cast<std::size_t>(columns_));
    for (int column = 0; column < columns_; ++column) {
        slots.push_back(slotOf(GridPoint{row, column}));
    }
    return slots;
}

std::vector<std::size_t> GridGeometry::rectangleSlots(std::size_t first, std::size_t second) const {
    const GridPoint a = pointOf(first);
    const GridPoint b = pointOf(second);
    const int topRow = std::min(a.row, b.row);
    const int bottomRow = std::max(a.row, b.row);
    const int leftColumn = std::min(a.column, b.column);
    const int rightColumn = std::max(a.column, b.column);

    std::vector<std::size_t> slots;
    slots.reserve(static_cast<std::size_t>((bottomRow - topRow + 1) * (rightColumn - leftColumn + 1)));
    for (int row = topRow; row <= bottomRow; ++row) {
        for (int column = leftColumn; column <= rightColumn; ++column) {
            slots.push_back(slotOf(GridPoint{row, column}));
        }
    }
    return slots;
}
