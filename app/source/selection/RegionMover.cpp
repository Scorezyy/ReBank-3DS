#include "selection/RegionMover.hpp"

bool RegionMover::fitsInside(const RegionClipboard& region, const GridGeometry& grid, GridPoint anchor) {
    const GridPoint low{anchor.row + region.minOffset().row, anchor.column + region.minOffset().column};
    const GridPoint high{anchor.row + region.maxOffset().row, anchor.column + region.maxOffset().column};
    return grid.contains(low) && grid.contains(high);
}

std::optional<StoragePane> RegionMover::neighbourPane(StorageAddress from, CursorDirection direction) {
    if (from.trash) {
        return std::nullopt;
    }
    if (from.pane == StoragePane::Local && direction == CursorDirection::Up) {
        return StoragePane::Cloud;
    }
    if (from.pane == StoragePane::Local && direction == CursorDirection::Right) {
        return StoragePane::Party;
    }
    if (from.pane == StoragePane::Cloud && direction == CursorDirection::Down) {
        return StoragePane::Local;
    }
    if (from.pane == StoragePane::Party && direction == CursorDirection::Left) {
        return StoragePane::Local;
    }
    return std::nullopt;
}

std::optional<RegionMover::Landing> RegionMover::enterPane(const RegionClipboard& region, StoragePane pane,
                                                           GridPoint desired, CursorDirection direction) {
    const GridGeometry grid = GridGeometry::forPane(pane);
    const GridPoint span = region.span();
    if (span.row > grid.rows() || span.column > grid.columns()) {
        return std::nullopt;
    }

    GridPoint anchor = desired;
    if (direction == CursorDirection::Up) {
        anchor.row = grid.rows() - 1 - region.maxOffset().row;
    } else if (direction == CursorDirection::Down) {
        anchor.row = -region.minOffset().row;
    } else if (direction == CursorDirection::Left) {
        anchor.column = grid.columns() - 1 - region.maxOffset().column;
    } else if (direction == CursorDirection::Right) {
        anchor.column = -region.minOffset().column;
    } else {
        return std::nullopt;
    }

    return Landing{StorageAddress{pane, false},
                   grid.clampToGrid(anchor, region.minOffset(), region.maxOffset())};
}

bool RegionMover::move(RegionClipboard& region, CursorDirection direction) const {
    if (direction == CursorDirection::None || region.empty()) {
        return false;
    }

    const GridPoint step = stepOf(direction);
    const GridPoint desired{region.anchor().row + step.row, region.anchor().column + step.column};
    if (fitsInside(region, region.grid(), desired)) {
        region.moveTo(region.location(), desired);
        return true;
    }

    const std::optional<StoragePane> neighbour = neighbourPane(region.location(), direction);
    if (!neighbour) {
        return false;
    }
    const std::optional<Landing> landing = enterPane(region, *neighbour, desired, direction);
    if (!landing) {
        return false;
    }
    region.moveTo(landing->address, landing->anchor);
    return true;
}
