#pragma once

#include "selection/CursorDirection.hpp"
#include "selection/RegionClipboard.hpp"

#include <optional>

class RegionMover {
public:
    bool move(RegionClipboard& region, CursorDirection direction) const;

private:
    struct Landing {
        StorageAddress address;
        GridPoint anchor;
    };

    static bool fitsInside(const RegionClipboard& region, const GridGeometry& grid, GridPoint anchor);
    static std::optional<StoragePane> neighbourPane(StorageAddress from, CursorDirection direction);
    static std::optional<Landing> enterPane(const RegionClipboard& region, StoragePane pane,
                                            GridPoint desired, CursorDirection direction);
};
