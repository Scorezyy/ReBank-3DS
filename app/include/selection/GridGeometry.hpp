#pragma once

#include "bank/BankTypes.hpp"

#include <cstddef>
#include <vector>

struct GridPoint {
    int row = 0;
    int column = 0;
};

constexpr bool operator==(GridPoint left, GridPoint right) {
    return left.row == right.row && left.column == right.column;
}

constexpr bool operator!=(GridPoint left, GridPoint right) {
    return !(left == right);
}

class GridGeometry {
public:
    static GridGeometry forPane(StoragePane pane);

    int columns() const { return columns_; }
    int rows() const { return rows_; }
    std::size_t slotCount() const { return static_cast<std::size_t>(columns_ * rows_); }

    GridPoint pointOf(std::size_t slot) const;
    std::size_t slotOf(GridPoint point) const;
    bool contains(GridPoint point) const;
    GridPoint clampToGrid(GridPoint point, GridPoint minOffset, GridPoint maxOffset) const;

    std::vector<std::size_t> rowSlots(int row) const;
    std::vector<std::size_t> rectangleSlots(std::size_t first, std::size_t second) const;

private:
    constexpr GridGeometry(int columns, int rows) : columns_(columns), rows_(rows) {}

    int columns_;
    int rows_;
};
