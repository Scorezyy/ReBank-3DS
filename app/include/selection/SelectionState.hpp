#pragma once

#include "selection/RegionClipboard.hpp"

#include <cstddef>
#include <cstdint>

enum class SelectionMode : std::uint8_t {
    Single,
    Row,
    Area
};

enum class SelectionPhase : std::uint8_t {
    Idle,
    Marking,
    Holding
};

struct SelectionState {
    SelectionMode mode = SelectionMode::Single;
    SelectionPhase phase = SelectionPhase::Idle;
    std::size_t anchorSlot = 0;
    RegionClipboard region;
};
